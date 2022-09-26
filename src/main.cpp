#include <Arduino.h>
#include <ACAN2515.h>

// Main Header
#include <main.h>

//——————————————————————————————————————————————————————————————————————————————
//  Task Handles
//——————————————————————————————————————————————————————————————————————————————

TaskHandle_t HeatingHousekeepingHandle;
TaskHandle_t UpdateLedsHandle;
TaskHandle_t ReadTemperaturesHandle;
TaskHandle_t PublishTemperaturesHandle;
TaskHandle_t PublishHeatingHandle;
TaskHandle_t PublishWaterHandle;
TaskHandle_t PublishStatusHandle;
TaskHandle_t ReconnectMqttHandle;
TaskHandle_t RegularMqttHandle;

TaskHandle_t SetEconomyModeHandle;
TaskHandle_t SetGuidanceHandle;
TaskHandle_t SetFeedTemperatureTaskHandle;
TaskHandle_t SetDhwNowHandle;
TaskHandle_t SetDateTimeHandle;
TaskHandle_t KeepAliveHandle;
TaskHandle_t SetDhwSetpointHandle;
TaskHandle_t SyncTimeTaskHandle;

//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

// This flag enables the control of the heating. It will be automatically reset to FALSE if another controller sends messages
//   It will be re-enabled if there are no messages from other controllers on the network for x seconds as defined by ControllerMessageTimeout
bool Override = true;

// Controller Message Timeout
//   After this timeout this controller will take over control.
int controllerMessageTimeout = 30000;

// Set this to true to view debug info
bool Debug = true;

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

//-- WiFi Status Timer Variable
unsigned long wifiConnectMillis = 0L;

//-- Last Controller Message timer
unsigned long controllerMessageTimer = 0L;

//-- Timestamp of last received message from the heating controller
unsigned long lastHeatingMessageTime = 0L;

//-- Timestamp of the last message sent by us
unsigned long lastSentMessageTime = 0L;

//-- Date & Time Interval: 0...MAXINT, Ex.: '5' for a 5 second delay between setting time.
int dateTimeSendDelay = 30;

void setup()
{
  // Setup Serial
  Serial.begin(115200);

  log_i("Starting up");

  // Read configuration
  bool result = ReadConfiguration();

  if (!result)
  {
    log_e("Unable to read configuration.");
    return;
  }

  Debug = configuration.General.Debug;
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

  // Connect WiFi. This call will block the thread until a result of the connection attempt has been received. This is very important for OTA to work.
  connectWifi();
  //-------------------------------------
  //-- NOTE: The code below won't be reached until the WiFi has connected within connectWifi().

  setupMqttClient();

  // Setup can module
  setupCan();


  ota();
  TelnetServer.begin();
  initSensors();
  lastHeatingMessageTime = millis();
  lastSentMessageTime = millis();

  //——————————————————————————————————————————————————————————————————————————————
  //  Setup Tasks
  //——————————————————————————————————————————————————————————————————————————————

  xTaskCreate(SyncTimeIfRequired, "Sync Time NTP", 2000, NULL, 5, &SyncTimeTaskHandle);

  // Request Temperatures and report them back to the MQTT broker
  //   Note: If 85.00° is shown or "unreachable" then the wiring is bad.
  if (configuration.Features.Features_AuxilaryParameters)
  {
    log_v("Creating Task %s", "ReadTemperatures");
    xTaskCreate(ReadTemperatures, "Read Temperature Sensors", 2000, NULL, 1, &ReadTemperaturesHandle);
  }

  if (configuration.Features.Features_HeatingParameters)
  {
    log_v("Creating Task %s", "PublishHeatingTemperatures");
    xTaskCreate(PublishHeatingTemperatures, "Pub Heat Temp", 3000, NULL, 1, &PublishHeatingHandle);
  }

  if (configuration.Features.Features_WaterParameters)
  {
    log_v("Creating Task %s", "PublishWaterTemperatures");
    xTaskCreate(PublishWaterTemperatures, "Pub HW Temp", 3000, NULL, 1, &PublishWaterHandle);
  }
  log_v("Creating Task %s", "PublishStatus");
  xTaskCreate(PublishStatus, "Pub Status", 3000, NULL, 1, &PublishStatusHandle);
  log_v("Creating Task %s", "UpdateLeds");
  // Task for updating LEDs
  xTaskCreate(UpdateLeds, "Update LEDS", 2000, NULL, 5, &UpdateLedsHandle);
  log_v("Creating Task %s", "HeatingHousekeeping");
  // Task for tracking boost function and override
  xTaskCreate(HeatingHousekeeping, "Heating Housekeeping", 1800, NULL, 1, &HeatingHousekeepingHandle);
  log_v("Creating Task %s", "reconnectMqtt");
  // Task for MQTT connection
  xTaskCreate(reconnectMqtt, "Reconnect MQTT", 2000, NULL, 1, &ReconnectMqttHandle);
  // Task for KeepAlive message 0x09F
  log_v("Creating Task %s", "KeepAlive");
  xTaskCreate(KeepAlive, "KeepAlive", 2000, NULL, 1, &KeepAliveHandle);
  // Task for Economy Mode
  log_v("Creating Task %s", "SetEconomyMode");
  xTaskCreate(SetEconomyMode, "Set Eco Mode", 2000, NULL, 1, &SetEconomyModeHandle);
  // Task for setting the current temperature guidance mode
  log_v("Creating Task %s", "SetGuidance");
  xTaskCreate(SetGuidance, "Set Guidance", 3000, NULL, 1, &SetGuidanceHandle);
  // Task for setting the desired feed temperature (Heating Circuit)
  log_v("Creating Task %s", "SetFeedTemperature");
  xTaskCreate(SetFeedTemperature, "Set Feed Temp", 3000, NULL, 1, &SetFeedTemperatureTaskHandle);
  // Task for setting the "Direct Hot Water Now" mode
  log_v("Creating Task %s", "SetDhwNow");
  xTaskCreate(SetDhwNow, "Set DHW Now", 2000, NULL, 1, &SetDhwNowHandle);
  // Task for updating date and time on the heating
  log_v("Creating Task %s", "SetDateTime");
  xTaskCreate(SetDateTime, "Set Date and Time", 3000, NULL, 1, &SetDateTimeHandle);
  // Task for setting the "Direct Hot Water" Setpoint
  log_v("Creating Task %s", "SetDhwSetpoint");
  xTaskCreate(SetDhwSetpoint, "Set DHW Setpt.", 2000, NULL, 1, &SetDhwSetpointHandle);
  // Task for flashing the status LED to show running state.
  log_v("Creating Task %s", "Heartbeat");
  xTaskCreate(ShowHeartbeat, "Heartbeat", 1000, NULL, 1, NULL);
}

void loop()
{
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
}

void FallbackTimekeeper(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  while (true)
  {
    // Wait before sending anything
    while (!SafeToSendMessage)
      vTaskDelay(100 / portTICK_PERIOD_MS);

    // Run on fallback values when the connection to the server has been lost.
    if (TimeIsSynced() && !client.connected())
    {

      // Note: negate this statement to try out the fallback mode.
      if (!client.connected() && !ceraValues.Fallback.isOnFallback)
      {
        // Activate fallback
        ceraValues.Fallback.isOnFallback = true;
        log_w("Connection lost. Switching over to fallback mode!");
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
      log_i("Connection established. Switching over to SCADA!");

    }

    if (TimeIsSynced() && !AlarmIsSet)
    {
      // Set Reboot time next day
      setEvent(Reboot, now() + 24 * 3600);
      AlarmIsSet = true;
    }
    // Wait 60 seconds
    vTaskDelay(random(60000/62000) / portTICK_PERIOD_MS);
  }
}

void Reboot()
{
  ESP.restart();
}

void SendMessage(CANMessage msg)
{
  // Send message if not empty and override is true.
  if (msg.id != 0 && Override)
  {
    if (Debug)
    {
      log_i("Sending CAN Message");
      WriteMessage(msg);
    }
    bool sendResult = can.tryToSend(msg);

    if (!sendResult)
    {
      log_e("Failed to send message with id [\e[1;32m0x%.3X\e[0m]. This has happened %i times.", msg.id, CanTransmitErrors++);
    }
    lastSentMessageTime = millis();
  }
}

void SetEconomyMode(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Apply some delay to stretch out task execution
  vTaskDelay(random(1000/5000) / portTICK_PERIOD_MS);
  // Run Task forever
  while (true)
  {

    CANMessage msg;
    // Switch economy mode. This is always the opposite of the desired operational state
    msg = PrepareMessage(configuration.CanAddresses.Heating.Economy, 1);
    msg.data[0] = !commandedValues.Heating.Active;
    if (Debug)
    {
      log_i("Heating Economy: %s", commandedValues.Heating.Active ? "yes" : "no");
    }
    // Wait before sending anything
    while (!SafeToSendMessage)
      vTaskDelay(100 / portTICK_PERIOD_MS);
    SendMessage(msg);
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    // Wait 30 Seconds until next execution.
    vTaskDelay(random(30000,32000) / portTICK_PERIOD_MS);
  }
}

void SetGuidance(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Apply some delay to stretch out task execution
  vTaskDelay(random(1000/5000) / portTICK_PERIOD_MS);
  while (true)
  {
    CANMessage msg;
    msg = PrepareMessage(configuration.CanAddresses.Heating.Mode, 1);
    msg.data[0] = 1;
    // Wait before sending anything
    while (!SafeToSendMessage)
      vTaskDelay(100 / portTICK_PERIOD_MS);
    SendMessage(msg);
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(random(30000,32000) / portTICK_PERIOD_MS);
  }
}

void SetDhwNow(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Apply some delay to stretch out task execution
  vTaskDelay(random(1000/5000) / portTICK_PERIOD_MS);
  while (true)
  {
    CANMessage msg;
    msg = PrepareMessage(configuration.CanAddresses.HotWater.Now, 1);
    msg.data[0] = 0x01;
    // Wait before sending anything
    while (!SafeToSendMessage)
      vTaskDelay(100 / portTICK_PERIOD_MS);
    SendMessage(msg);
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(random(30000,32000) / portTICK_PERIOD_MS);
  }
}

void SetDhwSetpoint(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Apply some delay to stretch out task execution
  vTaskDelay(random(1000/5000) / portTICK_PERIOD_MS);
  while (true)
  {
    CANMessage msg;
    msg = PrepareMessage(configuration.CanAddresses.HotWater.SetpointTemperature, 1);
    msg.data[0] = 20;
    // Wait before sending anything
    while (!SafeToSendMessage)
      vTaskDelay(100 / portTICK_PERIOD_MS);
    SendMessage(msg);
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(random(30000,32000) / portTICK_PERIOD_MS);
  }
}

void KeepAlive(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Apply some delay to stretch out task execution
  vTaskDelay(random(1000/5000) / portTICK_PERIOD_MS);
  while (true)
  {

    CANMessage msg;
    msg = PrepareMessage(0xF9, 0);
    // Wait before sending anything
    while (!SafeToSendMessage)
      vTaskDelay(100 / portTICK_PERIOD_MS);
    SendMessage(msg);
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(random(30000,32000) / portTICK_PERIOD_MS);
  }
}

void WriteMessage(CANMessage msg)
{
  char printbuf[255];
  // Buffer for storing the formatted values. We have to expect 'FF (255)' which is 8 bytes + 1 for string overhead \0
  char dataBuf[32];
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

  // Output to console
  log_i("[%s]\t\e[0mCAN: [\e[1;32m0x%.3X\e[0m] Data:\t%s", myTZ.dateTime("d-M-y H:i:s.v").c_str(), msg.id, data.c_str());
}

void SetDateTime(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Apply some delay to stretch out task execution
  vTaskDelay(random(1000/5000) / portTICK_PERIOD_MS);
  while (true)
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
      if (Debug)
      {
        log_i("[%s]\tDate and Time DOW:%i H:%i M:%i", myTZ.dateTime("d-M-y H:i:s.v").c_str(), myTZ.dateTime("N").toInt(), myTZ.hour(), myTZ.minute());
      }
      // Wait before sending anything
      while (!SafeToSendMessage)
        vTaskDelay(100 / portTICK_PERIOD_MS);
      SendMessage(msg);
    }
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay((dateTimeSendDelay + random(1,6)) * 1000 / portTICK_PERIOD_MS);
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

void UpdateLeds(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Task runs infinitely
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

    // Blink MQTT LED
    if (!client.connected())
    {
      digitalWrite(configuration.LEDs.MqttLed, !digitalRead(configuration.LEDs.MqttLed));
    }
    else
    {
      digitalWrite(configuration.LEDs.MqttLed, HIGH);
    }

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
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    // 1 Second Delay
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void ShowHeartbeat(void *pvParameter)
{
  while(true)
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

/// @brief Track boost function, enable override on foreign controller timeout
/// @param parameter
void HeatingHousekeeping(void *parameter)
{
  log_v("[%s]\t Begin Task", myTZ.dateTime("d-M-y H:i:s.v").c_str());
  // Task runs in infinite loop
  while (true)
  {
    // Boost Function
    if (commandedValues.Heating.Boost)
    {
      // Countdown to zero and switch off boost if 0
      if (commandedValues.Heating.BoostTimeCountdown > 0)
      {
        commandedValues.Heating.BoostTimeCountdown--;
        if (Debug)
        {
          log_i("[%s]\t BOOST: Time: %i Left: %i", myTZ.dateTime("d-M-y H:i:s.v").c_str(), commandedValues.Heating.BoostDuration, commandedValues.Heating.BoostTimeCountdown);
        }
      }
      else
      {
        commandedValues.Heating.Boost = false;
      }
    }

    // If we didn't spot a controller message on the network for x seconds we will take over control.
    // As soon as a message is spotted on the network it will be disabled again. This is controlled within processCan()
    if (millis() - controllerMessageTimer >= configuration.General.BusMessageTimeout * 1000)
    {
      // Bail out if we already set this...
      if (!Override)
      {
        Override = true;
        log_i("[%s]\t No other controller on the network. Enabling Override.", myTZ.dateTime("d-M-y H:i:s.v").c_str());
      }
    }
    log_v("Stack Diff: %i", uxTaskGetStackHighWaterMark(NULL));
    // Wait 1 second.
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

int _log_vprintf(const char *fmt, va_list args)
{
  if (TelnetRemoteClient.connected() && TelnetRemoteClient.available() == 0)
  {
    TelnetRemoteClient.printf(fmt, args);
  }

  return vprintf(fmt, args);
}