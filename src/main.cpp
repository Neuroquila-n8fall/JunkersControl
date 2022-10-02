#include <Arduino.h>
#include <ACAN2515.h>

// Main Header
#include <main.h>

//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

// This flag enables the control of the heating. It will be automatically reset to FALSE if another controller sends messages
//   It will be re-enabled if there are no messages from other controllers on the network for x seconds as defined by ControllerMessageTimeout
bool OverrideControl = true;

// Controller Message Timeout
//   After this timeout this controller will take over control.
int controllerMessageTimeout = 30000;

// This will be overwritten by Configuration!
bool DebugMode = true;

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
  // Init SPIFFS
  if (!LittleFS.begin())
    LittleFS.begin(true);
  // Setup Serial
  Serial.begin(115200);
  Serial.printf("\e[1;32mRunning Environment: %s\r\n\e[0m", STR(ENV));

  Serial.printf("\e[1;36mPress the \"BOOT\" button within the next 5 seconds to enable Setup Mode!\r\n\e[0m", STR(ENV));

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

    if (!LittleFS.exists("/configuration.json"))
    {
      Serial.println("\e[1;31mPlease upload the Filesystem image first.\e[0m");
      return;
    }
    // Launch AP Mode to let the user configure the basics.
    StartApMode();
    ConfigureAndStartWebserver();
    ota();
    return;
  }

#pragma endregion

Serial.printf("\e[1;36mSetup Mode not enabled. You can enable it at every time by pressing the \"BOOT\" button once. \r\n\e[0m", STR(ENV));

  // Read configuration
  bool result = ReadConfiguration();

  if (!result)
  {
    Log.println("Unable to read configuration.");
    return;
  }

  DebugMode = configuration.General.Debug;
  controllerMessageTimeout = configuration.General.BusMessageTimeout;

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

  // Connect WiFi. This call will block the thread until a result of the connection attempt has been received. This is very important for OTA to work.
  connectWifi();
  //-------------------------------------
  //-- NOTE: The code below won't be reached until the WiFi has connected within connectWifi().
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

  // Run Timer Events
  events();
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

  // MQTT Client Keepalive
  client.loop();
  // Process incoming CAN messages
  processCan();
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
    // Ensure that we are connected to MQTT
    reconnectMqtt();

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
    if (SafeToSendMessage)
    {

      // Send desired Values to the heating controller
      // Note that it cannot perform unrealistic actions.
      // The built-in controller of the heating will always take care of staying well within the specs
      // We can only "suggest" to set to a certain temperature or switching off the pump(s)

      // I have "borrowed" the concept of a step-chain from PLC programming since it appears
      //   to have been incoorporated into the controller as well because values arrive in
      //   intervals of approximately 1 second.

      CANMessage msg;

      double feedTemperature = 0.0F;
      int feedSetpoint = 0;

      switch (currentStep)
      {
      case 0:
        // Switch economy mode. This is always the opposite of the desired operational state
        msg = PrepareMessage(configuration.CanAddresses.Heating.Economy, 1);
        msg.data[0] = !commandedValues.Heating.Active;
        if (DebugMode)
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

        if (DebugMode)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Heating is %s, Fallback is %s\r\n", currentStep, ceraValues.Heating.Active ? "ON" : "OFF", ceraValues.Fallback.isOnFallback ? "YES" : "NO");
        }

        break;

      // DHW "Now"
      case 3:
        msg = PrepareMessage(configuration.CanAddresses.HotWater.Now, 1);
        msg.data[0] = 0x01;
        if (DebugMode)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Set DHW Now to %s\r\n", currentStep, ceraValues.Hotwater.Now ? "ON" : "OFF");
        }
        break;

      // DHW Temperature Setpoint
      case 4:
        msg = PrepareMessage(configuration.CanAddresses.HotWater.SetpointTemperature, 1);
        msg.data[0] = 20;
        if (DebugMode)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Set DHW Setpoint to %i\r\n", currentStep, ceraValues.Hotwater.SetPoint);
        }
        break;

      case 5:
        // Request? Data
        msg = PrepareMessage(0xF9, 0);
        if (DebugMode)
        {
          Log.printf("DEBUG STEP CHAIN #%i: Sending KeepAlive\r\n", currentStep);
        }
        break;

      default:
        // If we reach any undefined number inside the chain, reset to zero
        currentStep = 0;
        return; // important!
      }

      // Increase counter
      currentStep++;

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
    // Run on fallback values when the connection to the server has been lost.
    if (TimeIsSynced() && !client.connected())
    {

      // Note: negate this statement to try out the fallback mode.
      if (!client.connected() && !ceraValues.Fallback.isOnFallback)
      {
        // Activate fallback
        ceraValues.Fallback.isOnFallback = true;
        Log.println("Connection lost. Switching over to fallback mode!");
      }

      // Check if the profile has to be changed depending on the time schedule.
      //   Check if the current hour is in between the start of both "Start" and "End" marks
      if (myTZ.hour() >= ceraValues.Fallback.fallbackStartEntry.StartHour && myTZ.hour() < ceraValues.Fallback.fallbackEndEntry.StartHour)
      {
        // Check if the minute mark has been passed.
        if (myTZ.minute() >= ceraValues.Fallback.fallbackStartEntry.StartMinute)
        {
          // Activate Heating Profile by overwriting the fields with fallback values
          commandedValues.Heating.BasepointTemperature = ceraValues.Fallback.BasepointTemperature;
          commandedValues.Heating.EndpointTemperature = ceraValues.Fallback.EndpointTemperature;
          commandedValues.Heating.Active = true;
          return; // important!
        }
      }

      // Check if we have passed the hour mark.
      if (myTZ.hour() > ceraValues.Fallback.fallbackEndEntry.StartHour)
      {
        // Check if the minute mark has been passed
        if (myTZ.minute() >= ceraValues.Fallback.fallbackStartEntry.StartMinute)
        {
          // Set both Base and Endpoint to the anti-freeze setting.
          commandedValues.Heating.BasepointTemperature = ceraValues.Fallback.MinimumFeedTemperature;
          commandedValues.Heating.EndpointTemperature = ceraValues.Fallback.MinimumFeedTemperature;
          commandedValues.Heating.Active = false;
        }
      }
    }

    // Disable fallback mode when connected.
    if (client.connected() && ceraValues.Fallback.isOnFallback)
    {
      ceraValues.Fallback.isOnFallback = false;
      Log.println("Connection established. Switching over to SCADA!");
    }

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
    if (DebugMode)
    {
      Log.printf("DEBUG STEP CHAIN #%i: Sending CAN Message\r\n", currentStep);
      WriteMessage(msg);
    }
    if (!can.tryToSend(msg))
    {
      CanSendErrorCount++;
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

        char logMsg[50];
        sprintf(logMsg, "CAN send error CLEARED", msg.id, CanSendErrorCount);
        PublishLog(logMsg, __func__, LogLevel::Info);
        Log.printf("\e[0;32[%s] CAN send error CLEARED after %i previously failed attempts.\r\n\e[0m", myTZ.dateTime("d-M-y H:i:s.v").c_str(), CanSendErrorCount);
        CanSendErrorCount = 0;
      }
    }
    lastSentMessageTime = millis();
  }
}

void WriteMessage(CANMessage msg)
{
  // Buffer for storing the formatted values. We have to expect 'FF (255)' which is 8 bytes + 1 for string overhead \0
  char dataBuf[255];
  String data;

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
    // Add tab between data
    if (x < msg.len - 1)
    {
      data += "\t";
    }
  }

  Log.printf("[%s]\t\e[0mCAN: [\e[1;32m0x%.3X\e[0m] Data:\t%s\r\n", myTZ.dateTime("d-M-y H:i:s.v").c_str(), msg.id, data.c_str());
}

void SetDateTime()
{
  char printbuf[255];
  runEverySeconds(dateTimeSendDelay)
  {
    if (lastSentMessageTime - millis() >= 1000)
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
      if (DebugMode)
      {
        Log.printf("DEBUG: Date and Time DOW:%i H:%i M:%i\r\n", myTZ.dateTime("N").toInt(), myTZ.hour(), myTZ.minute());
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
    return (lastSentMessageTime - millis() >= 1000);

  return (lastHeatingMessageTime - millis() >= 1000 && lastSentMessageTime - millis() >= 1000);
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
      // BLink LED if Pump is active but heating isn't. This means the heating is about to go off.
      if (ceraValues.Heating.PumpActive && !ceraValues.Heating.Active)
      {
        digitalWrite(configuration.LEDs.HeatingLed, !digitalRead(configuration.LEDs.HeatingLed));
      }

      if (!ceraValues.Heating.PumpActive && !ceraValues.Heating.Active)
      {
        digitalWrite(configuration.LEDs.HeatingLed, LOW);
      }

      if (ceraValues.Heating.PumpActive && ceraValues.Heating.Active)
      {
        digitalWrite(configuration.LEDs.HeatingLed, HIGH);
      }
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
        if (DebugMode)
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