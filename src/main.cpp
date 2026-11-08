#include <Arduino.h>
#include <ACAN2515.h>

// Main Header
#include <main.h>
#include <failsafe.h>
#include <forecast.h>

//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

// Start read-only. Control is enabled only after a complete detection timeout
// without room-controller traffic, preventing startup noise on an occupied bus.
bool OverrideControl = false;

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

TaskHandle_t MqttActivityHandle;
TaskHandle_t CanErrorActivityHandle;

//-- WiFi Status Timer Variable
unsigned long wifiConnectMillis = 0L;

//-- Last Controller Message timer
unsigned long controllerMessageTimer = 0L;

//-- Timestamp of last received message from the heating controller
unsigned long lastHeatingMessageTime = 0L;

//-- Timestamp of the last message sent by us
unsigned long lastSentMessageTime = 0L;

//-- Step-Counter
int currentStep = 0;

//-- Date & Time Interval: 0...MAXINT, Ex.: '5' for a 5 second delay between setting time.
int dateTimeSendDelay = 30;

//-- CAN Error Counter
volatile int CanSendErrorCount;

volatile bool SetupMode = false;
static unsigned long lastHeartbeatMillis = 0;

static bool HeartbeatIsDue()
{
  const unsigned long interval = configuration.CanModuleConfig.HeartbeatIntervalSeconds * 1000UL;
  return millis() - lastHeartbeatMillis >= interval;
}

static void SendConfiguredHeartbeat()
{
  if (configuration.CanModuleConfig.ReadOnly || !OverrideControl ||
      !HeartbeatIsDue() || !SafeToSendMessage())
    return;
  CANMessage msg = PrepareMessage(configuration.CanModuleConfig.HeartbeatAddress, 0);
  LogDebug("CAN: heartbeat 0x%.3X", configuration.CanModuleConfig.HeartbeatAddress);
  SendMessage(msg);
  lastHeartbeatMillis = millis();
}

void setup()
{
  // Setup Serial
  // A TX buffer lets Serial.write return before the line is on the wire, keeping the log lock short.
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  LogBegin();
  LogInfo("Build: %s · env: %s", JC_STRINGIFY(VERSION), JC_STRINGIFY(ENV));

  // Init LittleFS. Formatting remains the recovery path for an unreadable
  // partition, but report it explicitly because it removes all stored files.
  if (!LittleFS.begin())
  {
    LogWarn("LittleFS: mount failed, formatting partition");
    if (!LittleFS.begin(true))
    {
      LogError("LittleFS: mount and format failed, check partition layout and flash a matching littlefs.bin");
      return;
    }
    LogWarn("LittleFS: formatted and empty, upload littlefs.bin");
  }

  String restoreError;
  if (!RestoreConfigurationAfterFilesystemUpdate(restoreError))
    LogError("Config: %s", restoreError.c_str());

  LogInfo("Press BOOT within 5 s for Setup Mode (hold 10 s to factory-reset configuration)");

#pragma region "Setup Mode"

  bool factoryResetRequested = false;
  const unsigned long setupWindowStarted = millis();
  // A short press selects Setup Mode. A deliberately long hold additionally
  // clears both persistent NVS configuration and the LittleFS copy.
  while (millis() - setupWindowStarted <= 5000)
  {
    if (!digitalRead(GPIO_NUM_0))
    {
      SetupMode = true;
      const unsigned long buttonHeldSince = millis();
      while (!digitalRead(GPIO_NUM_0) && millis() - buttonHeldSince < 10000)
        delay(20);
      factoryResetRequested = millis() - buttonHeldSince >= 10000;
      break;
    }
    delay(10);
  }

  if (factoryResetRequested)
  {
    String resetError;
    if (!ClearPersistentConfigurationBackup(resetError))
    {
      LogError("Config: factory reset aborted: %s", resetError.c_str());
    }
    else
    {
      LittleFS.remove("/configuration.json");
      LittleFS.remove("/configuration.bak");
      LittleFS.remove("/configuration.tmp");
      LittleFS.remove("/configuration.restore");
      SetConfigurationUploadPending(false);
      LogWarn("Config: factory reset done, persistent and LittleFS configuration cleared");
    }
  }

  if (SetupMode)
  {

    const bool frontendAvailable = LittleFS.exists("/frontend/index.html") ||
                                   LittleFS.exists("/frontend/index.html.gz");
    if (!frontendAvailable)
    {
      LogError("LittleFS: web frontend missing, upload a matching littlefs.bin");
      return;
    }
    if (!LittleFS.exists("/configuration.json"))
      LogWarn("Config: /configuration.json missing, starting Setup Mode for upload");
    // Launch AP Mode to let the user configure the basics.
    StartApMode();
    ConfigureAndStartWebserver();
    ota();
    return;
  }

#pragma endregion

  LogInfo("Setup Mode off, press BOOT once to enter it at any time");

  // Read configuration
  bool result = ReadConfiguration();

  if (!result)
  {
    const bool frontendAvailable = LittleFS.exists("/frontend/index.html") ||
                                   LittleFS.exists("/frontend/index.html.gz");
    if (!frontendAvailable)
    {
      LogError("Config: load failed and web frontend missing, upload a matching littlefs.bin");
      return;
    }

    LogWarn("Config: missing or invalid, starting setup access point");
    SetupMode = true;
    StartApMode();
    ConfigureAndStartWebserver();
    ota();
    return;
  }

  if (configuration.CanModuleConfig.ReadOnly)
  {
    OverrideControl = false;
    LogWarn("CAN: read-only mode, no bus messages will be sent");
  }

  SetupFailSafe();
  if (!myTZ.setPosix(configuration.General.PosixTimezone))
    LogWarn("Config: invalid POSIX timezone, fail-safe uses boiler time or its unknown-time policy");

  // Setup Pins
  pinMode(configuration.LEDs.StatusLed, OUTPUT);
  pinMode(configuration.LEDs.WifiLed, OUTPUT);
  pinMode(configuration.LEDs.MqttLed, OUTPUT);
  pinMode(configuration.LEDs.HeatingLed, OUTPUT);

  // Test Leds
  digitalWrite(configuration.LEDs.StatusLed, HIGH);
  delay(1000);
  digitalWrite(configuration.LEDs.WifiLed, HIGH);
  delay(1000);
  digitalWrite(configuration.LEDs.MqttLed, HIGH);
  delay(1000);
  digitalWrite(configuration.LEDs.HeatingLed, HIGH);
  delay(100);
  digitalWrite(configuration.LEDs.WifiLed, LOW);
  delay(100);
  digitalWrite(configuration.LEDs.MqttLed, LOW);
  delay(100);
  digitalWrite(configuration.LEDs.HeatingLed, LOW);

  setupMqttClient();

  // Setup can module
  setupCan();

  // Start WiFi asynchronously. Heating/CAN processing remains active while it connects.
  connectWifi();
  ota();
  TelnetServer.begin();
  initSensors();
  controllerMessageTimer = millis();
  lastHeatingMessageTime = millis();
  lastSentMessageTime = millis();

  xTaskCreate(ReadTemperatures, "Read Aux Temp", 4096, NULL, 5, NULL);

  xTaskCreate(ShowHeartbeat, "Heartbeat LED", 1024, NULL, 5, NULL);

  xTaskCreate(ShowMqttActivity, "MQTT Activity", 2048, NULL, 5, &MqttActivityHandle);

  xTaskCreate(UpdateLeds, "Update LEDs", 2048, NULL, 5, NULL);

  xTaskCreate(TrackBoostFunction, "Track Boost", 2048, NULL, 1, NULL);

  ConfigureAndStartWebserver();
}

void loop()
{
  // First, so the live log keeps flowing in Setup Mode, during OTA and before a requested reboot.
  LogService();
  // Init reboot if requested
  if (ShouldReboot) {
    // Wait a bit so the client is redirected properly...
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    WiFi.disconnect();
    server->end();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    // Ensure all filesystem metadata and file contents are committed before a
    // software reset. This mirrors the reliable power-cycle behavior.
    LittleFS.end();
    ESP.restart();
  }

  // Stop executing when SetupMode is active.
  if (SetupMode)
  {
    // But we like to be still able to update files via OTA ofc
    ArduinoOTA.handle();
    ProcessForecastCorrection();
    return;
  }

  // Check if the user has pressed the "BOOT" button
  if (digitalRead(GPIO_NUM_0) == LOW)
  {
    SetupMode = true;
    // Disconnect Wifi and launch in AP Mode
    StartApMode();
    return;
  }

  // Connect WiFi (if disconnected)
  connectWifi();
  //-------------------------------------
  //-- NOTE: The code below won't be reached until the WiFi has connected within connectWifi().
  // Handle OTA
  ArduinoOTA.handle();

  // Break out if OTA is in Progress
  if (otaRunning)
  {
    return;
  }

  // Process incoming CAN messages
  processCan();
  // Apply the local safety profile before the next CAN command-chain step.
  UpdateFailSafe();
  // A sustained manufacturer-defined 10 C feed request means heating off.
  // This is a millis-based state machine and does not add a task or scheduler.
  UpdateHeatingSetpointShutdown();
  // MQTT maintenance follows CAN work and is bounded by the configured socket
  // timeout, so network traffic cannot take priority over boiler traffic.
  client.loop();
  ProcessPendingRuntimeControls();
  ProcessForecastCorrection();
  // Coalesce stable MQTT/HA control changes before writing configuration. This
  // stays in the main loop and avoids both flash churn and another task.
  ProcessRuntimeControlPersistence();
  // Telnet Communication and system log
  CheckForConnections();
  ReadFromTelnet();
  // Set Date & Time
  SetDateTime();

  //——————————————————————————————————————————————————————————————————————————————
  // Actions performed every second
  //——————————————————————————————————————————————————————————————————————————————
  runEverySeconds(1)
  {
    // If we didn't spot a controller message on the network for x seconds we will take over control.
    // As soon as a message is spotted on the network it will be disabled again. This is controlled within processCan()
    // Read the clock here: processCan() above may have stamped a frame after any earlier reading,
    // and the unsigned difference would wrap and enable override while a controller is active.
    if (!configuration.CanModuleConfig.ReadOnly &&
        millis() - controllerMessageTimer >= configuration.General.BusMessageTimeout * 1000)
    {
      // Bail out if we already set this...
      if (!OverrideControl)
      {
        OverrideControl = true;
        LogInfo("CAN: no other controller, override enabled");
      }
    }
  }

  //——————————————————————————————————————————————————————————————————————————————
  // Control Actions
  //——————————————————————————————————————————————————————————————————————————————

  // Keep all control activity in the main loop. The heartbeat is checked here
  // rather than in a scheduler task and uses the same spacing guard as all
  // other CAN traffic.
  SendConfiguredHeartbeat();

  // Enabled functional profiles
  // determine which parts of the installation may be driven.
  runEverySeconds(5)
  {
    // We will send our data if there was silence on the bus for a specific time. This prevents sending uneccessary payload onto the bus or confusing the boiler if it's slow and brittle.
    if (SafeToSendMessage())
    {

      // Send desired Values to the heating controller
      // Note that it cannot perform unrealistic actions.
      // The built-in controller of the heating will always take care of staying well within the specs
      // We can only "suggest" to set to a certain temperature or switching off the pump(s)

      // I have "borrowed" the concept of a step-chain from PLC programming since it appears
      //   to have been incorporated into the controller as well because values arrive in
      //   intervals of approximately 1 second.

      CANMessage msg = {};

      switch (currentStep)
      {
      case 0:
        if (!configuration.CanModuleConfig.Profiles.Heating)
          break;
        // Switch economy mode. This is always the opposite of the desired operational state
        msg = PrepareMessage(configuration.CanAddresses.Heating.Economy, 1);
        msg.data[0] = !commandedValues.Heating.Active;
        LogDebug("CAN step #%i: economy %d", currentStep, !commandedValues.Heating.Active);
        break;

      // Temperature regulation mode
      //  1 = Weather guided | 0 = Room Temperature Guided
      case 1:
        if (!configuration.CanModuleConfig.Profiles.Heating)
          break;
        msg = PrepareMessage(configuration.CanAddresses.Heating.Mode, 1);
        msg.data[0] = 1;
        break;

      case 2:
        if (!configuration.CanModuleConfig.Profiles.Heating)
          break;
        SetFeedTemperature();

        LogDebug("CAN step #%i: heating %s, fail-safe %s", currentStep, ceraValues.Heating.Active ? "on" : "off", IsFailSafeActive() ? "yes" : "no");

        break;

      // DHW "Now"
      case 3:
        if (!configuration.CanModuleConfig.Profiles.DomesticHotWater)
          break;
        msg = PrepareMessage(configuration.CanAddresses.HotWater.Now, 1);
        msg.data[0] = 0x01;
        LogDebug("CAN step #%i: DHW now %s", currentStep, ceraValues.Hotwater.Now ? "on" : "off");
        break;

      // DHW Temperature Setpoint
      case 4:
        if (!configuration.CanModuleConfig.Profiles.DomesticHotWater)
          break;
        msg = PrepareMessage(configuration.CanAddresses.HotWater.SetpointTemperature, 1);
        msg.data[0] = static_cast<uint8_t>(commandedValues.HotWater.SetPoint * 2);
        LogDebug("CAN step #%i: DHW setpoint → %i", currentStep, commandedValues.HotWater.SetPoint);
        break;

      case 5:
        // Reserved step. The independently timed heartbeat above may use this
        // gap without constraining its interval to a 30-second multiple.
        break;

      default:
        // If we reach any undefined number inside the chain, reset to zero
        currentStep = 0;
        return; // important!
      }

      // Increase counter and keep the chain within its six defined steps.
      currentStep = (currentStep + 1) % 6;

      SendMessage(msg);
    }
  }

  //——————————————————————————————————————————————————————————————————————————————
  // Actions performed every five seconds
  //——————————————————————————————————————————————————————————————————————————————
  runEverySeconds(5)
  {

    // Publish Status
    PublishStatus();

    // Request Temperatures and report them back to the MQTT broker
    //   Note: If 85.00° is shown or "unreachable" then the wiring is bad.
    if (configuration.Features.AuxiliaryParameters)
    {
      PublishAuxiliaryTemperatures();
    }

    // Publish Heating Temperatures
    if (configuration.Features.HeatingParameters)
      PublishHeatingTemperaturesAndStatus();

    // Publish Water Temperatures
    if (configuration.Features.WaterParameters)
      PublishWaterTemperatures();
  }

  //——————————————————————————————————————————————————————————————————————————————
  // Actions performed every thirty seconds
  //——————————————————————————————————————————————————————————————————————————————
  runEverySeconds(30)
  {
    if (TimeIsSynced() && !AlarmIsSet)
    {
      // Set Reboot time next day
      setEvent(Reboot, now() + 24 * 3600);
      AlarmIsSet = true;
    }
  }

  runEverySeconds(60)
  {
    // Set Date & Time
    SetDateTime();
  }
  runEverySeconds(1)
  {
    // Network recovery deliberately runs after all CAN and control work.
    reconnectMqtt();
  }
  // Progress ezTime only after CAN receive and control work has completed.
  // Unlike waitForSync(), this returns after the current event attempt.
  events();
  // Allow the CPU to switch tasks.
  vTaskDelay(2);
}

void Reboot()
{
  ESP.restart();
}

void SendMessage(CANMessage msg)
{
  // ReadOnly is a hard interlock and cannot be undone by the controller
  // timeout logic. Address zero remains reserved as the empty-message marker.
  if (msg.id != 0 && OverrideControl && !configuration.CanModuleConfig.ReadOnly)
  {
    LogDebug("CAN step #%i: sending", currentStep);
    if (!can.tryToSend(msg))
    {
      CanSendErrorCount = CanSendErrorCount + 1;
      if (CanErrorActivityHandle == NULL)
      {
        xTaskCreate(ShowCanError, "Can Error", 2000, NULL, 1, &CanErrorActivityHandle);
      }
      // One Error per streak (and at powers of ten) keeps a CAN outage from evicting the stored history.
      if (CanSendErrorCount == 1 || CanSendErrorCount == 10 || CanSendErrorCount == 100 || CanSendErrorCount == 1000)
        LogError("CAN: send 0x%.3X failed (%i in a row)", msg.id, CanSendErrorCount);
      else
        LogDebug("CAN: send 0x%.3X failed (%i in a row)", msg.id, CanSendErrorCount);
      char logMsg[64];
      sprintf(logMsg, "CAN send error msg id [0x%.3X]. Err Count: %i", msg.id, CanSendErrorCount);
      PublishLog(logMsg, __func__, LogLevel::Error);
    }
    else
    {
      WriteMessage(msg, false);

      if (CanErrorActivityHandle != NULL)
      {
        vTaskDelete(CanErrorActivityHandle);
        CanErrorActivityHandle = NULL;

        PublishLog("CAN send error CLEARED", __func__, LogLevel::Info);
        LogInfo("CAN: send error cleared after %i failed attempts", CanSendErrorCount);
        CanSendErrorCount = 0;
      }
    }
    lastSentMessageTime = millis();
  }
}

void WriteMessage(CANMessage msg, bool received /* = true */)
{
  JsonDocument doc;
  doc["id"] = msg.id;
  doc["len"] = msg.len;
  doc["rcv"] = received;
  doc["ts"] = millis();
  JsonArray msgData = doc["data"].to<JsonArray>();

  for (int x = 0; x < msg.len; x++)
    msgData.add((int)msg.data[x]);

  String json;
  serializeJson(doc, json);
  if (eventSource)
    eventSource->send(json.c_str(), "can");
  if (configuration.General.Debug || configuration.General.Sniffing)
  {
    // "0xFF(255)" plus separator is 10 characters; a classic CAN frame has at most 8 bytes.
    char data[88];
    size_t used = 0;
    for (int x = 0; x < msg.len && used + 10 < sizeof(data); x++)
      used += snprintf(data + used, sizeof(data) - used, x ? " 0x%.2X(%u)" : "0x%.2X(%u)", msg.data[x], msg.data[x]);
    data[used] = '\0';
    LogCan("CAN: %s 0x%.3X  %s", received ? "◄" : "►", msg.id, data);
  }
}

void SetDateTime()
{
  // Before the first NTP sync the clock still reads the 1970 power-on default
  // (Thursday 01:00); sending it would set a wrong time on the boiler.
  if (timeStatus() == timeNotSet)
    return;
  runEverySeconds(dateTimeSendDelay)
  {
    if (millis() - lastSentMessageTime >= 1000)
    {

      CANMessage msg = PrepareMessage(configuration.CanAddresses.General.DateTime, 4);

      // Get day of week:
      //  --> N = ISO-8601 numeric representation of the day of the week. (1 = Monday, 7 = Sunday)
      msg.data[0] = myTZ.dateTime("N").toInt();
      // Hours and minutes
      msg.data[1] = myTZ.hour();
      msg.data[2] = myTZ.minute();
      // As of now we don't know what this value is for but it seems mandatory.
      msg.data[3] = 4;
      LogDebug("CAN: date/time DOW %u %02u:%02u", msg.data[0], msg.data[1], msg.data[2]);

      SendMessage(msg);
    }
  }
}

CANMessage PrepareMessage(uint32_t id, int length /* = 8 */)
{
  CANMessage msg;
  // This was the culprit of messages not arriving as they should.
  // We have to set up the length of the message first. The heating doesn't care about that much but the library does!
  msg.len = length;
  // These are here for reference only and are the default values of the ctr
  msg.ext = false;
  msg.rtr = false;
  msg.idx = 0;
  msg.id = id;
  return msg;
}

/// @brief Returns if the last sent or received message was a second away
/// @param dontWaitForController Just check for the last message timestamp we sent and not this of the controller.
/// @return
bool SafeToSendMessage(bool dontWaitForController /*= true*/)
{
  if (dontWaitForController)
    return (millis() - lastSentMessageTime >= 1000);

  return (millis() - lastHeatingMessageTime >= 1000 && millis() - lastSentMessageTime >= 1000);
}

void ShowHeartbeat(void *pvParameter)
{
  while (true)
  {
    digitalWrite(configuration.LEDs.StatusLed, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(configuration.LEDs.StatusLed, LOW);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    digitalWrite(configuration.LEDs.StatusLed, HIGH);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(configuration.LEDs.StatusLed, LOW);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void ShowMqttActivity(void *pvParameter)
{
  digitalWrite(configuration.LEDs.MqttLed, LOW);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  digitalWrite(configuration.LEDs.MqttLed, HIGH);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  digitalWrite(configuration.LEDs.MqttLed, LOW);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  digitalWrite(configuration.LEDs.MqttLed, HIGH);
  MqttActivityHandle = NULL;
  vTaskDelete(NULL);
}

void ShowCanError(void *pvParameter)
{
  while (true)
  {
    digitalWrite(configuration.LEDs.HeatingLed, !digitalRead(configuration.LEDs.HeatingLed));
    vTaskDelay(500);
  }
}

void UpdateLeds(void *pvParameter)
{
  while (true)
  {
    // Blink Wifi LED
    if (!WiFi.isConnected())
    {
      digitalWrite(configuration.LEDs.WifiLed, !digitalRead(configuration.LEDs.WifiLed));
    }
    else
    {
      digitalWrite(configuration.LEDs.WifiLed, HIGH);
    }

    if (MqttActivityHandle == NULL)
    {
      // Blink MQTT LED
      if (!client.connected())
      {
        digitalWrite(configuration.LEDs.MqttLed, !digitalRead(configuration.LEDs.MqttLed));
      }
      else
      {
        digitalWrite(configuration.LEDs.MqttLed, HIGH);
      }
    }

    if (CanErrorActivityHandle == NULL)
    {
      // The heating LED is the flame indicator. CAN errors retain priority and
      // blink this output from ShowCanError instead.
      digitalWrite(configuration.LEDs.HeatingLed, ceraValues.General.FlameLitKnown && ceraValues.General.FlameLit ? HIGH : LOW);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TrackBoostFunction(void *pvParameter)
{
  while (true)
  {
    // Boost Function
    if (commandedValues.Heating.Boost)
    {
      // Countdown to zero and switch off boost if 0
      if (commandedValues.Heating.BoostTimeCountdown > 0)
      {
        commandedValues.Heating.BoostTimeCountdown--;
        LogDebug("Boost: %i of %i s left", commandedValues.Heating.BoostTimeCountdown, commandedValues.Heating.BoostDuration);
      }
      else
      {
        commandedValues.Heating.Boost = false;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
