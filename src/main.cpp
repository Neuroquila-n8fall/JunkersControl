#include <Arduino.h>
#include <ACAN2515.h>

// Main Header
#include <main.h>
// General Configuration
#include <configuration.h>
// Include MQTT Topics
#include <mqtt.h>
// CAN Module Settings
#include <can_processor.h>
// Telnet
#include <telnet.h>
// Heating Parameters
#include <heating.h>
// WiFi
#include <wifi_config.h>
// OTA
#include <ota.h>
// Temperature Sensors
#include <t_sensors.h>
// NTP Timesync
#include <timesync.h>


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

//-- Step-Counter
int currentStep = 0;

//-- Date & Time Interval: 0...MAXINT, Ex.: '5' for a 5 second delay between setting time.
int dateTimeSendDelay = 30;
// LED Helper Variables
bool statusLed = false;
bool wifiLed = false;
bool mqttLed = false;

void setup()
{
  // Setup Serial
  Serial.begin(115200);

  // Read configuration
  bool result = ReadConfiguration();

  if (!result)
  {
    WriteToConsoles("Unable to read configuration.");
    return;
  }

  Debug = configuration.General.Debug;
  controllerMessageTimeout = configuration.General.BusMessageTimeout;

  // Resize the Temperature array
  ceraValues.Auxilary.Temperatures = (double *)malloc(configuration.TemperatureSensors.SensorCount * sizeof(double));

  // Setup Pins
  pinMode(configuration.LEDs.StatusLed, OUTPUT);
  pinMode(configuration.LEDs.WifiLed, OUTPUT);
  pinMode(configuration.LEDs.MqttLed, OUTPUT);
  digitalWrite(configuration.LEDs.StatusLed, HIGH);
  statusLed = true;
  delay(1000);
  digitalWrite(configuration.LEDs.WifiLed, HIGH);
  delay(1000);
  digitalWrite(configuration.LEDs.MqttLed, HIGH);
  delay(100);
  digitalWrite(configuration.LEDs.WifiLed, LOW);
  delay(100);
  digitalWrite(configuration.LEDs.MqttLed, LOW);

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
  // Set Date & Time
  SetDateTime();

  //
  runEveryMilliseconds(500)
  {
    if (ceraValues.Heating.PumpActive && ceraValues.Heating.Active)
    {
      digitalWrite(configuration.LEDs.HeatingLed, !digitalRead(configuration.LEDs.HeatingLed));
    }
  }

  //——————————————————————————————————————————————————————————————————————————————
  // Actions performed every second
  //——————————————————————————————————————————————————————————————————————————————
  runEverySeconds(1)
  {
    char printbuf[255];
    // Ensure that we are connected to MQTT
    reconnectMqtt();

    // Blink Wifi LED
    if (!WiFi.isConnected())
    {
      digitalWrite(configuration.LEDs.WifiLed, wifiLed ? HIGH : LOW);
      wifiLed = !wifiLed;
    }
    else
    {
      digitalWrite(configuration.LEDs.WifiLed, HIGH);
      wifiLed = true;
    }

    // Blink MQTT LED
    if (!client.connected())
    {
      digitalWrite(configuration.LEDs.MqttLed, mqttLed ? HIGH : LOW);
      mqttLed = !mqttLed;
    }
    else
    {
      digitalWrite(configuration.LEDs.MqttLed, HIGH);
      mqttLed = true;
    }

    if (ceraValues.Heating.PumpActive && !ceraValues.Heating.Active)
    {
      digitalWrite(configuration.LEDs.HeatingLed, !digitalRead(configuration.LEDs.HeatingLed));
    }

    if (!ceraValues.Heating.PumpActive && !ceraValues.Heating.Active)
    {
      digitalWrite(configuration.LEDs.HeatingLed, 0);
    }

    // Boost Function
    if (commandedValues.Heating.Boost)
    {
      // Countdown to zero and switch off boost if 0
      if (commandedValues.Heating.BoostTimeCountdown > 0)
      {
        commandedValues.Heating.BoostTimeCountdown--;
        if (Debug)
        {
          sprintf(printbuf, "DEBUG BOOST: Time: %i Left: %i \r\n", commandedValues.Heating.BoostDuration, commandedValues.Heating.BoostTimeCountdown);
          String message(printbuf);
          WriteToConsoles(message);
        }
      }
      else
      {
        commandedValues.Heating.Boost = false;
      }
    }

    // If we didn't spot a controller message on the network for x seconds we will take over control.
    // As soon as a message is spotted on the network it will be disabled again. This is controlled within processCan()
    if (currentMillis - controllerMessageTimer >= configuration.General.BusMessageTimeout * 1000)
    {
      // Bail out if we already set this...
      if (!Override)
      {
        Override = true;
        WriteToConsoles("No other controller on the network. Enabling Override.\r\n");
      }
    }
  }

  //——————————————————————————————————————————————————————————————————————————————
  // Control Actions
  //——————————————————————————————————————————————————————————————————————————————
  runEverySeconds(3)
  {
    // We will send our data if there was silence on the bus for a specific time. This prevents sending uneccessary payload onto the bus or confusing the boiler if it's slow and brittle.
    if (lastHeatingMessageTime - currentMillis >= 2000 && lastSentMessageTime - currentMillis >= 1000)
    {

      char printbuf[255];

      // Send desired Values to the heating controller
      // Note that it cannot perform unrealistic actions.
      // The built-in controller of the heating will always take care of staying well within the specs
      // We can only "suggest" to set to a certain temperature or switching off the pump(s)

      // I have "borrowed" the concept of a step-chain from PLC programming since it appears
      //   to have been incoorporated into the controller as well because values arrive in
      //   intervals of approximately 1 second.

      CANMessage msg = PrepareMessage(0x0);

      double feedTemperature = 0.0F;
      int feedSetpoint = 0;

      switch (currentStep)
      {
      case 0:
        // Switch economy mode. This is always the opposite of the desired operational state
        msg.id = 0x253;
        msg.data[0] = !commandedValues.Heating.Active;
        if (Debug)
        {
          sprintf(printbuf, "DEBUG STEP CHAIN #%i: Heating Economy: %d", currentStep, !commandedValues.Heating.Active);
          String message(printbuf);
          WriteToConsoles(message + "\r\n");
        }
        break;

      // Temperature regulation mode
      //  1 = Weather guided | 0 = Room Temperature Guided
      case 1:
        msg.id = 0x258;
        msg.data[0] = 1;
        break;

      case 2:

        // Get raw Setpoint
        feedTemperature = CalculateFeedTemperature();
        // Transform it into the int representation
        feedSetpoint = ConvertFeedTemperature(feedTemperature);

        msg.id = 0x252;
        msg.data[0] = feedSetpoint;
        if (Debug)
        {
          sprintf(printbuf, "DEBUG STEP CHAIN #%i: Heating is %s, Fallback is %s, Feed Setpoint is %.2f, INT representation (half steps) is %i", currentStep, ceraValues.Heating.Active ? "ON" : "OFF", ceraValues.Fallback.isOnFallback ? "YES" : "NO", feedTemperature, feedSetpoint);
          String message(printbuf);
          WriteToConsoles(message + "\r\n");
        }

        break;

      // DHW "Now"
      case 3:
        msg.id = 0x254;
        msg.data[0] = 0x01;
        break;

      // DHW Temperature Setpoint
      case 4:
        msg.id = 0x255;
        msg.data[0] = 20;
        break;

      case 5:
        // Request? Data
        msg.id = 0xF9;
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
    if (configuration.Features.Features_AuxilaryParameters)
    {
      ReadTemperatures();
      PublishAuxilaryTemperatures();
    }

    // Publish Heating Temperatures
    if (configuration.Features.Features_HeatingParameters)
      PublishHeatingTemperatures();

    // Publish Water Temperatures
    if (configuration.Features.Features_WaterParameters)
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
      if (!Debug)
      {
        // Activate fallback
        ceraValues.Fallback.isOnFallback = true;
        WriteToConsoles("Connection lost. Switching over to fallback mode!\r\n");
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
          ceraValues.Heating.Active = true;
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
          ceraValues.Heating.Active = false;
        }
      }
    }

    // Disable fallback mode when connected.
    if (client.connected() && ceraValues.Fallback.isOnFallback)
    {
      ceraValues.Fallback.isOnFallback = false;
      WriteToConsoles("Connection established. Switching over to SCADA!\r\n");
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
}

void Reboot()
{
  ESP.restart();
}

void SendMessage(CANMessage msg)
{
  char printbuf[255];
  // Send message if not empty and override is true.
  if (msg.id != 0 && Override)
  {
    if (Debug)
    {
      sprintf(printbuf, "DEBUG STEP CHAIN #%i: Sending CAN Message", currentStep);
      String message(printbuf);
      WriteToConsoles(message + "\r\n");

      WriteMessage(msg);
    }
    can.tryToSend(msg);
    lastSentMessageTime = millis();


  }
}

void WriteMessage(CANMessage msg)
{
  char printbuf[255];
  // Buffer for storing the formatted values. We have to expect 'FF (255)' which is 8 bytes + 1 for string overhead \0
  char dataBuf[12];
  String data;

  for (int x = 0; x < msg.len; x++)
  {
    // A little bit of trickery to assemble the data bytes into a nicely formatted string
    sprintf(dataBuf, "0x%.2X (%i)", msg.data[x], msg.data[x]);
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

  // Print string
  sprintf(printbuf, "CAN: [0x%.3X] Data:\t", msg.id);
  String consoleMessage(printbuf);
  consoleMessage = myTZ.dateTime("[d-M-y H:i:s.v] - ") + consoleMessage;
  consoleMessage += data;
  WriteToConsoles(consoleMessage);
  WriteToConsoles("\r\n");
}

void SetDateTime()
{
  char printbuf[255];
  runEverySeconds(dateTimeSendDelay)
  {
    if (lastSentMessageTime - millis() >= 1000)
    {
      CANMessage msg = PrepareMessage(0x256);

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
        sprintf(printbuf, "DEBUG: Date and Time DOW:%i H:%i M:%i", myTZ.dateTime("N").toInt(), myTZ.hour(), myTZ.minute());
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
      }

      SendMessage(msg);
    }
  }
}

CANMessage PrepareMessage(uint32_t id)
{
  CANMessage msg;
  // This was the culprit of messages not arriving as they should.
  // We have to set up the length of the message first. The heating doesn't care about that much but the library does!
  msg.len = 8;
  // These are here for reference only and are the default values of the ctr
  msg.ext = false;
  msg.rtr = false;
  msg.idx = 0;
  msg.id = id;
  return msg;
}