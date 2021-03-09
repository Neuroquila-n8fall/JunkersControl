#include <Arduino.h>
#include <ACAN2515.h>

//Main Header
#include <main.h>
//Include MQTT Topics
#include <mqtt.h>
//CAN Module Settings
#include <can_processor.h>
//Telnet
#include <telnet.h>
//Heating Parameters
#include <heating.h>
//WiFi
#include <wifi_config.h>
//OTA
#include <ota.h>
//Temperature Sensors
#include <t_sensors.h>
//NTP Timesync
#include <timesync.h>


//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

//This flag enables the control of the heating. It will be automatically reset to FALSE if another controller sends messages
//  It will be re-enabled if there are no messages from other controllers on the network for x seconds as defined by ControllerMessageTimeout
bool Override = true;

//Controller Message Timeout
//  After this timeout this controller will take over control.
const int controllerMessageTimeout = 30000;

//Set this to true to view debug info
bool Debug = true;

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————



//-- WiFi Status Timer Variable
unsigned long wifiConnectMillis = 0L;

//-- One-Second Interval Timer Variable
unsigned long oneSecondTimer = 0L;

//-- Five-Second Interval Timer Variable
unsigned long fiveSecondTimer = 0L;

//-- Thirty-Second Interval Timer Variable
unsigned long thirtySecondTimer = 0L;

//-- Last Controller Message timer
unsigned long controllerMessageTimer = 0L;

//-- Step-Counter
int currentStep = 0;


void setup()
{
  setupMqttClient();
  //Setup Serial
  Serial.begin(115200);

  //Setup can module
  setupCan();

  //Connect WiFi. This call will block the thread until a result of the connection attempt has been received. This is very important for OTA to work.
  connectWifi();
  //-------------------------------------
  //-- NOTE: The code below won't be reached until the WiFi has connected within connectWifi().
  ota();
  TelnetServer.begin();
  initSensors();

}

void loop()
{
  //store the current timer millis
  unsigned long currentMillis = millis();
  //Connect WiFi (if disconnected)
  connectWifi();
  //-------------------------------------
  //-- NOTE: The code below won't be reached until the WiFi has connected within connectWifi().
  //Handle OTA
  ArduinoOTA.handle();

  //Break out if OTA is in Progress
  if (otaRunning)
  {
    return;
  }

  //MQTT Client Keepalive
  client.loop();
  //Process incoming CAN messages
  processCan();
  //Telnet Communication
  CheckForConnections();
  //Read Telnet commands
  ReadFromTelnet();
  //——————————————————————————————————————————————————————————————————————————————
  //Actions performed every second
  //——————————————————————————————————————————————————————————————————————————————
  if (currentMillis - oneSecondTimer >= 1000)
  {
    char printbuf[255];
    oneSecondTimer = currentMillis;
    //Ensure that we are connected to MQTT
    reconnectMqtt();

    //Boost Function
    if (mqttBoost)
    {
      //Countdown to zero and switch off boost if 0
      if (boostTimeCountdown > 0)
      {
        boostTimeCountdown--;
        if (Debug)
        {
          sprintf(printbuf, "DEBUG BOOST: Time: %i Left: %i \r\n", mqttBoostDuration, boostTimeCountdown);
          String message(printbuf);
          WriteToConsoles(message);
        }
      }
      else
      {
        mqttBoost = false;
      }
    }

    //If we didn't spot a controller message on the network for x seconds we will take over control.
    //As soon as a message is spotted on the network it will be disabled again. This is controlled within processCan()
    if (currentMillis - controllerMessageTimer >= controllerMessageTimeout)
    {
      //Bail out if we already set this...
      if (!Override)
      {
        Override = true;
        WriteToConsoles("No other controller on the network. Enabling Override.\r\n");
      }
    }

    //Send desired Values to the heating controller
    //Note that it cannot perform unrealistic actions.
    //The built-in controller of the heating will always take care of staying well within the specs
    //We can only "suggest" to set to a certain temperature or switching off the pump(s)

    //I have "borrowed" the concept of a step-chain from PLC programming since it appears
    //  to have been incoorporated into the controller as well because values arrive in
    //  intervals of approximately 1 second.

    CANMessage msg;
    //This was the culprit of messages not arriving as they should.
    //We have to set up the length of the message first. The heating doesn't care about that much but the library does!
    msg.len = 8;
    //These are here for reference only and are the default values of the ctr
    msg.ext = false;
    msg.rtr = false;
    msg.idx = 0;

    double feedTemperature = 0.0F;
    int feedSetpoint = 0;
    switch (currentStep)
    {
    case 0:
      break;

    case 1:
      //Send Date
      msg.id = 0x256;
      //Get day of week:
      // --> N = ISO-8601 numeric representation of the day of the week. (1 = Monday, 7 = Sunday)
      msg.data[0] = myTZ.dateTime("N").toInt();
      //Hours and minutes
      msg.data[1] = myTZ.hour();
      msg.data[2] = myTZ.minute();
      //As of now we don't know what this value is for but it seems mandatory.
      msg.data[3] = 4;
      if (Debug)
      {
        sprintf(printbuf, "DEBUG STEP CHAIN #%i: Date and Time DOW:%i H:%i M:%i", currentStep, myTZ.dateTime("N").toInt(), myTZ.hour(), myTZ.minute());
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
      }
      break;

    case 2:
      //Switch economy mode. This is always the opposite of the desired operational state
      msg.id = 0x253;
      msg.data[0] = !mqttHeatingSwitch;
      if (Debug)
      {
        sprintf(printbuf, "DEBUG STEP CHAIN #%i: Heating Economy: %d", currentStep, !mqttHeatingSwitch);
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
      }
      break;

    //Temperature regulation mode
    // 1 = Weather guided | 0 = Room Temperature Guided
    case 3:
      msg.id = 0x258;
      msg.data[0] = 1;
      break;

    case 4:

      //Get raw Setpoint
      feedTemperature = CalculateFeedTemperature();
      //Transform it into the int representation
      feedSetpoint = ConvertFeedTemperature(feedTemperature);

      msg.id = 0x252;
      msg.data[0] = feedSetpoint;
      if (Debug)
      {
        sprintf(printbuf, "DEBUG STEP CHAIN #%i: Heating is %s, Fallback is %s, Feed Setpoint is %.2f, INT representation (half steps) is %i", currentStep, hcActive ? "ON" : "OFF", isOnFallback ? "YES" : "NO", feedTemperature, feedSetpoint);
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
      }
      //Report Values back to the Broker if on Override.
      if (Override)
      {
        //Report back Operational State
        client.publish(pub_HcOperation, hcActive ? "1" : "0");
        //Report back feed setpoint
        client.publish(pub_SetpointFeedTemperature, String(feedTemperature).c_str());
        //Report back Boost status
        client.publish(pub_Boost, mqttBoost ? "1" : "0");
        //Report back Fastheatup status
        client.publish(pub_Fastheatup, mqttFastHeatup ? "1" : "0");
      }

      break;

    //DHW "Now"
    case 5:
      msg.id = 0x254;
      msg.data[0] = 0x01;
      break;

    //DHW Temperature Setpoint
    case 6:
      msg.id = 0x255;
      msg.data[0] = 20;
      break;

    case 7:
      //Request? Data
      msg.id = 0xF9;
      break;

    default:
      //If we reach any undefined number inside the chain, reset to zero
      currentStep = 0;
      return; // important!
    }

    //Send message if not empty and override is true.
    if (msg.id != 0 && Override)
    {
      if (Debug)
      {
        sprintf(printbuf, "DEBUG STEP CHAIN #%i: Sending CAN Message", currentStep);
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
      }
      can.tryToSend(msg);

      //Buffer for storing the formatted values. We have to expect 'FF (255)' which is 8 bytes + 1 for string overhead \0
      char dataBuf[9];
      String data;

      for (int x = 0; x < msg.len; x++)
      {
        //A little bit of trickery to assemble the data bytes into a nicely formatted string
        sprintf(dataBuf, "%02X (%i)", msg.data[x], msg.data[x]);
        //Convert char array to string
        String temp(dataBuf);
        //Get rid of trailing spaces
        temp.trim();
        //Concat
        data += temp;
        //Add tab between data
        if (x < msg.len - 1)
        {
          data += "\t";
        }
      }

      //Print string
      sprintf(printbuf, "CAN: [%04X] Data:\t", msg.id);
      String consoleMessage(printbuf);
      consoleMessage = myTZ.dateTime("[d-M-y H:i:s.v] - ") + consoleMessage;
      consoleMessage += data;
      WriteToConsoles(consoleMessage);
      WriteToConsoles("\r\n");
    }

    //Increase counter
    currentStep++;
  }

  //——————————————————————————————————————————————————————————————————————————————
  //Actions performed every five seconds
  //——————————————————————————————————————————————————————————————————————————————
  if (currentMillis - fiveSecondTimer >= 5000)
  {
    fiveSecondTimer = currentMillis;

    //Request remperatures and report them back to the MQTT broker
    //  Note: If 85.00° is shown or "unreachable" then the wiring is bad.
    ReadAndSendTemperatures();
  }

  //——————————————————————————————————————————————————————————————————————————————
  //Actions performed every thirty seconds
  //——————————————————————————————————————————————————————————————————————————————
  if (currentMillis - thirtySecondTimer >= 30000)
  {
    thirtySecondTimer = currentMillis;

    // Run on fallback values when the connection to the server has been lost.
    if (TimeIsSynced() && !client.connected())
    {

      //Note: negate this statement to try out the fallback mode.
      if (!Debug)
      {
        //Activate fallback
        isOnFallback = true;
        WriteToConsoles("Connection lost. Switching over to fallback mode!\r\n");
      }

      //Check if the profile has to be changed depending on the time schedule.
      //  Check if the current hour is in between the start of both "Start" and "End" marks
      if (myTZ.hour() >= fallbackStartEntry.StartHour && myTZ.hour() < fallbackEndEntry.StartHour)
      {
        //Check if the minute mark has been passed.
        if (myTZ.minute() >= fallbackStartEntry.StartMinute)
        {
          //Activate Heating Profile by overwriting the fields with fallback values
          mqttBasepointTemperature = fallbackBasepointTemperature;
          mqttEndpointTemperature = fallbackEndpointTemperature;
          hcActive = true;
          return; //important!
        }
      }

      //Check if we have passed the hour mark.
      if (myTZ.hour() > fallbackEndEntry.StartHour)
      {
        //Check if the minute mark has been passed
        if (myTZ.minute() >= fallbackStartEntry.StartMinute)
        {
          //Set both Base and Endpoint to the anti-freeze setting.
          mqttBasepointTemperature = fallbackMinimumFeedTemperature;
          mqttEndpointTemperature = fallbackMinimumFeedTemperature;
          hcActive = false;
        }
      }
    }

    //Disable fallback mode when connected.
    if (client.connected() && isOnFallback)
    {
      isOnFallback = false;
      WriteToConsoles("Connection established. Switching over to SCADA!\r\n");
    }
  }
}











