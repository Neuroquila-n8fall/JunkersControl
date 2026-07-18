#include <Arduino.h>
#include <ACAN2515.h>

// Main Header
#include <main.h>
#include <failsafe.h>

//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

// This flag enables the control of the heating. It will be automatically reset to FALSE if another controller sends messages
//   It will be re-enabled if there are no messages from other controllers on the network for x seconds as defined by ControllerMessageTimeout
bool OverrideControl = true;

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

void setup()
{
  // Setup Serial
  Serial.begin(115200);
  Serial.printf("\e[1;32mRunning Environment: %s\r\n\e[0m", JC_STRINGIFY(ENV));
  Serial.printf("\e[1;32mRunning Build: %s\r\n\e[0m", JC_STRINGIFY(VERSION));

  // Init LittleFS. Formatting remains the recovery path for an unreadable
  // partition, but report it explicitly because it removes all stored files.
  if (!LittleFS.begin())
  {
    Serial.println("\e[1;33mLittleFS could not be mounted. Attempting to format the filesystem partition.\e[0m");
    if (!LittleFS.begin(true))
    {
      Serial.println("\e[1;31mLittleFS could not be mounted or formatted. Check the partition layout and flash a matching littlefs.bin.\e[0m");
      return;
    }
    Serial.println("\e[1;33mLittleFS was formatted and is empty. Upload littlefs.bin before continuing.\e[0m");
  }

  String restoreError;
  if (!RestoreConfigurationAfterFilesystemUpdate(restoreError))
    Serial.printf("\e[1;31m%s\e[0m\r\n", restoreError.c_str());

  Serial.println("\e[1;36mPress the \"BOOT\" button within the next 5 seconds to enable Setup Mode!\e[0m");

#pragma region "Setup Mode"

  unsigned long curmils = millis();
  // Give the user the chance to push the "BOOT" button.
  while (millis() - curmils <= 5000)
  {
    SetupMode = !digitalRead(GPIO_NUM_0);
    if (SetupMode)
    {
      break;
    }
  }

  if (SetupMode)
  {

    const bool frontendAvailable = LittleFS.exists("/frontend/index.html") ||
                                   LittleFS.exists("/frontend/index.html.gz");
    if (!frontendAvailable)
    {
      Serial.println("\e[1;31mLittleFS is mounted, but the web frontend is missing. Upload a matching littlefs.bin.\e[0m");
      return;
    }
    if (!LittleFS.exists("/configuration.json"))
      Serial.println("\e[1;33m/configuration.json is missing. Starting Setup Mode so it can be uploaded.\e[0m");
    // Launch AP Mode to let the user configure the basics.
    StartApMode();
    ConfigureAndStartWebserver();
    ota();
    return;
  }

#pragma endregion

Serial.println("\e[1;36mSetup Mode not enabled. You can enable it at every time by pressing the \"BOOT\" button once. \e[0m");

  // Read configuration
  bool result = ReadConfiguration();

  if (!result)
  {
    const bool frontendAvailable = LittleFS.exists("/frontend/index.html") ||
                                   LittleFS.exists("/frontend/index.html.gz");
    if (!frontendAvailable)
    {
      Serial.println("\e[1;31mConfiguration could not be loaded and the web frontend is missing. Upload a matching littlefs.bin.\e[0m");
      return;
    }

    Serial.println("\e[1;33mConfiguration is missing or invalid. Starting the CERASMARTER setup access point.\e[0m");
    SetupMode = true;
    StartApMode();
    ConfigureAndStartWebserver();
    ota();
    return;
  }

  SetupFailSafe();
  if (!myTZ.setPosix(configuration.General.PosixTimezone))
    Serial.println("Invalid POSIX timezone rule. Fail-safe will use boiler time or its unknown-time policy.");

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

  // store the current timer millis
  unsigned long currentMillis = millis();
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
  // MQTT maintenance follows CAN work and is bounded by the configured socket
  // timeout, so network traffic cannot take priority over boiler traffic.
  client.loop();
  // Telnet Communication
  CheckForConnections();
  // Read Telnet commands
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
    if (currentMillis - controllerMessageTimer >= configuration.General.BusMessageTimeout * 1000)
    {
      // Bail out if we already set this...
      if (!OverrideControl)
      {
        OverrideControl = true;
        Log.println("No other controller on the network. Enabling Override.");
      }
    }
  }

  //——————————————————————————————————————————————————————————————————————————————
  // Control Actions
  //——————————————————————————————————————————————————————————————————————————————

  // TODO: Seek for a more elegant solution to send each message every 30 seconds. Right now it's 5 because we have 6 Steps and we want an interval of 30 seconds so 30/6 = 5 seconds delay.
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
        // Switch economy mode. This is always the opposite of the desired operational state
        msg = PrepareMessage(configuration.CanAddresses.Heating.Economy, 1);
        msg.data[0] = !commandedValues.Heating.Active;
        if (configuration.General.Debug)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Heating Economy: %d\r\n", currentStep, !commandedValues.Heating.Active);
        }
        break;

      // Temperature regulation mode
      //  1 = Weather guided | 0 = Room Temperature Guided
      case 1:
        msg = PrepareMessage(configuration.CanAddresses.Heating.Mode, 1);
        msg.data[0] = 1;
        break;

      case 2:
        SetFeedTemperature();

        if (configuration.General.Debug)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Heating is %s, Fail-safe is %s\r\n", currentStep, ceraValues.Heating.Active ? "ON" : "OFF", IsFailSafeActive() ? "YES" : "NO");
        }

        break;

      // DHW "Now"
      case 3:
        msg = PrepareMessage(configuration.CanAddresses.HotWater.Now, 1);
        msg.data[0] = 0x01;
        if (configuration.General.Debug)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Set DHW Now to %s\r\n", currentStep, ceraValues.Hotwater.Now ? "ON" : "OFF");
        }
        break;

      // DHW Temperature Setpoint
      case 4:
        msg = PrepareMessage(configuration.CanAddresses.HotWater.SetpointTemperature, 1);
        msg.data[0] = 20;
        if (configuration.General.Debug)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Set DHW Setpoint to %.2F\r\n", currentStep, ceraValues.Hotwater.SetPoint);
        }
        break;

      case 5:
        // Request? Data
        msg = PrepareMessage(0xF9, 0);
        if (configuration.General.Debug)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Sending KeepAlive\r\n", currentStep);
        }
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
  // Send message if not empty and override is true.
  if (msg.id != 0 && OverrideControl)
  {
    if (configuration.General.Debug)
    {
      Log.printf("DEBUG STEP CHAIN #%i: Sending CAN Message\r\n", currentStep);
      WriteMessage(msg, false);
    }
    if (!can.tryToSend(msg))
    {
      CanSendErrorCount = CanSendErrorCount + 1;
      if (CanErrorActivityHandle == NULL)
      {
        xTaskCreate(ShowCanError, "Can Error", 2000, NULL, 1, &CanErrorActivityHandle);
      }
      Log.printf("\e[0;31[%s] Failed to send message [0x%.3X] over CAN. This has happened %i times before in a row.\r\n\e[0m", myTZ.dateTime("d-M-y H:i:s.v").c_str(), msg.id, CanSendErrorCount);
      char logMsg[64];
      sprintf(logMsg, "CAN send error msg id [0x%.3X]. Err Count: %i", msg.id, CanSendErrorCount);
      PublishLog(logMsg, __func__, LogLevel::Error);
    }
    else
    {

      if (CanErrorActivityHandle != NULL)
      {
        vTaskDelete(CanErrorActivityHandle);
        CanErrorActivityHandle = NULL;

        PublishLog("CAN send error CLEARED", __func__, LogLevel::Info);
        Log.printf("\e[0;32[%s] CAN send error CLEARED after %i previously failed attempts.\r\n\e[0m", myTZ.dateTime("d-M-y H:i:s.v").c_str(), CanSendErrorCount);
        CanSendErrorCount = 0;
      }
    }
    lastSentMessageTime = millis();
  }
}

void WriteMessage(CANMessage msg, bool received /* = true */)
{
  // Buffer for storing the formatted values. We have to expect 'FF (255)' which is 8 bytes + 1 for string overhead \0
  char dataBuf[255];
  String data;
  JsonDocument doc;
  doc["id"] = msg.id;
  doc["len"] = msg.len;
  doc["rcv"] = received;
  JsonArray msgData = doc["data"].to<JsonArray>();

  for (int x = 0; x < msg.len; x++)
  {
    // A little bit of trickery to assemble the data bytes into a nicely formatted string
    sprintf(dataBuf, "\e[1;35m0x%.2X \e[0m(\e[1;36m%i\e[0m)", msg.data[x], msg.data[x]);
    // Convert char array to string
    String temp(dataBuf);
    // Get rid of trailing spaces
    temp.trim();
    // Concat
    data += temp;

    msgData.add((int)msg.data[x]);
    // Add tab between data
    if (x < msg.len - 1)
    {
      data += "\t";
    }
  }
  String json;
  serializeJson(doc, json);
  eventSource->send(json.c_str(), "can");
  Log.printf("[%s]\t\e[0m[%s]CAN: [\e[1;32m0x%.3X\e[0m] Data:\t%s\r\n", myTZ.dateTime("d-M-y H:i:s.v").c_str(), received ? "\e[1;36m◄\e[0m" : "\e[1;35m►\e[0m", msg.id, data.c_str());
}

void SetDateTime()
{
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
      if (configuration.General.Debug)
      {
        Log.printf("DEBUG: Date and Time DOW:%li H:%i M:%i\r\n", myTZ.dateTime("N").toInt(), myTZ.hour(), myTZ.minute());
      }

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
      digitalWrite(configuration.LEDs.HeatingLed, ceraValues.General.FlameLit ? HIGH : LOW);
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
        if (configuration.General.Debug)
        {
          Log.printf("[%s][%s] Time: %i Left: %i \r\n", myTZ.dateTime("d-M-y H:i:s.v").c_str(), __func__, commandedValues.Heating.BoostDuration, commandedValues.Heating.BoostTimeCountdown);
        }
      }
      else
      {
        commandedValues.Heating.Boost = false;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
