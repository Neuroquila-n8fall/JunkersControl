#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <ACAN2515.h>
#include <eztime.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//Main Header
#include <main.h>
//Secrets
#include <arduino_secrets.h>
//Template Functions
#include <templates.h>
//Include MQTT Topics
#include <mqtt_config.h>
//CAN Module Settings
#include <can_config.h>
//Telnet
#include <telnet.h>
//Heating Parameters
#include <heating.h>
//WiFi
#include <wifi_config.h>


//——————————————————————————————————————————————————————————————————————————————
//  NTP Time Object
//——————————————————————————————————————————————————————————————————————————————
Timezone myTZ;

void setup()
{

  //Setup MQTT client
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
  client.setKeepAlive(10);
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
        sprintf(printbuf, "DEBUG STEP CHAIN #%i: Heating is %s, Fallback is %s, Feed Setpoint is %f, INT representation (half steps) is %i", currentStep, hcActive ? "ON" : "OFF", isOnFallback ? "YES" : "NO", feedTemperature, feedSetpoint);
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

//Simply takes all current states into account and dispatches the setpoint message immediately.
void SetFeedTemperature()
{
  CANMessage msg;
  //This was the culprit of messages not arriving as they should.
  //We have to set up the length of the message first. The heating doesn't care about that much but the library does!
  msg.len = 8;
  //These are here for reference only and are the default values of the ctr
  msg.ext = false;
  msg.rtr = false;
  msg.idx = 0;

  char printbuf[255];
  double feedTemperature = 0.0F;
  int feedSetpoint = 0;

  //Get raw Setpoint
  feedTemperature = CalculateFeedTemperature();
  //Transform it into the int representation
  feedSetpoint = ConvertFeedTemperature(feedTemperature);

  msg.id = 0x252;
  msg.data[0] = feedSetpoint;
  if (Debug)
  {
    sprintf(printbuf, "DEBUG SETFEEDTEMPERATURE: Feed Setpoint is %.2f, INT representation (half steps) is %i", feedTemperature, feedSetpoint);
    String message(printbuf);
    WriteToConsoles(message + "\r\n");
  }
  //Report Feed Temperature back
  if (Override)
  {
    client.publish(pub_SetpointFeedTemperature, String(feedTemperature).c_str());
  }
}

void setupCan()
{

  //Setup CAN Module
  SPI.begin(MCP2515_SCK, MCP2515_MISO, MCP2515_MOSI);
  ACAN2515Settings settings(QUARTZ_FREQUENCY, 10UL * 1000UL); // CAN bit rate 10 kb/s

  const uint16_t errorCode = can.begin(settings, [] { can.isr(); });
  if (errorCode == 0)
  {
    Serial.print("Bit Rate prescaler: ");
    Serial.println(settings.mBitRatePrescaler);
    Serial.print("Propagation Segment: ");
    Serial.println(settings.mPropagationSegment);
    Serial.print("Phase segment 1: ");
    Serial.println(settings.mPhaseSegment1);
    Serial.print("Phase segment 2: ");
    Serial.println(settings.mPhaseSegment2);
    Serial.print("SJW: ");
    Serial.println(settings.mSJW);
    Serial.print("Triple Sampling: ");
    Serial.println(settings.mTripleSampling ? "yes" : "no");
    Serial.print("Actual bit rate: ");
    Serial.print(settings.actualBitRate());
    Serial.println(" bit/s");
    Serial.print("Exact bit rate ? ");
    Serial.println(settings.exactBitRate() ? "yes" : "no");
    Serial.print("Sample point: ");
    Serial.print(settings.samplePointFromBitStart());
    Serial.println("%");
  }
  else
  {
    Serial.print("Configuration error 0x");
    Serial.println(errorCode, HEX);
  }
}

//Process incoming CAN messages
void processCan()
{
  CANMessage Message;
  if (can.receive(Message))
  {
    unsigned long curMillis = millis();
    //Buffer for sending console output. 100 chars should be enough for now:
    //[25-Aug-18 14:32:53.282]\tCAN: [0000] Data: FF (255)\tFF (255)\tFF (255)\tFF (255)\tFF (255)
    char printBuf[100];
    //Buffer for storing the formatted values. We have to expect 'FF (255)' which is 8 bytes + 1 for string overhead \0
    char dataBuf[9];
    String data;

    for (int x = 0; x < Message.len; x++)
    {
      //A little bit of trickery to assemble the data bytes into a nicely formatted string
      sprintf(dataBuf, "%02X (%i)", Message.data[x], Message.data[x]);
      //Convert char array to string
      String temp(dataBuf);
      //Get rid of trailing spaces
      temp.trim();
      //Concat
      data += temp;
      //Add tab between data
      if (x < Message.len - 1)
      {
        data += "\t";
      }
    }

    //Check for other controllers on the network by watching out for messages that are greater than 0x250
    if (Message.id > 0x250)
    {
      //Switch off override if another controller sends messages on the network.
      Override = false;
      controllerMessageTimer = curMillis;
      WriteToConsoles("Detected another controller on the network. Disabling Override\r\n");
    }

    //Print string
    sprintf(printBuf, "CAN: [%04X] Data:\t", Message.id);
    String consoleMessage(printBuf);
    consoleMessage = myTZ.dateTime("[d-M-y H:i:s.v] - ") + consoleMessage;
    consoleMessage += data;
    WriteToConsoles(consoleMessage);
    WriteToConsoles("\r\n");

    /*************************************
     * Terms
     * Controller = Built-in controller of your boiler
     * RC = The "Room Controller" or "Remote Control" or as I call it the "Consumer Controller" where you usually set up time schedules and temperatures.
     * HC = Heating Circuit
     * DHW = Domestic Hot Water
     * MC = Mixed Circuit
     * FT = Feed Temperature
     * Basepoint = Outside temperature at which the heating should deliver the highest possible feed temperature.
     * Endpoint = Outside temperature at which the heating should deliver the lowest possible feed temperature. Also known as "cut-off" temperature (depends on who you are talking with about this topic ;))
    **************************************/

    unsigned int rawTemp = 0;
    char errorCode[2];
    switch (Message.id)
    {

    //[HC] - [Controller] - Max. possible feed temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x200:
      temp = Message.data[0] / 2.0;
      hcMaxFeed = temp;
      client.publish(pub_HcMaxFeedTemperature, String(temp).c_str());
      break;

    //[HC] - [Controller] - Current feed temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x201:
      temp = Message.data[0] / 2.0;
      client.publish(pub_CurFeedTemperature, String(temp).c_str());
      break;

    //[DHW] - [Controller] - Max. possible water temperature -or- target temperature when running in heating battery mode
    //Data Type: INT
    //Value: Data / 2.0
    case 0x202:
      temp = Message.data[0] / 2.0;
      break;

    //[DHW] - [Controller] - Current water temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x203:
      temp = Message.data[0] / 2.0;
      break;

    //[DHW] - [Controller] - Max. water temperature (limited by boiler dial setting)
    //Data Type: INT
    //Value: Data / 2.0
    case 0x204:
      temp = Message.data[0] / 2.0;
      break;

    //[DHW] - [Controller] - Current water feed or battery temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x205:
      temp = Message.data[0] / 2.0;
      break;

    //[Controller] - Error Byte
    //Data Type: Byte
    //Value: 0x00 = Operational
    //       Error codes and their meaning vary between models. See your manual for details.
    case 0x206:
      status = Message.data[0];
      String(status).toCharArray(errorCode, 2);
      client.publish(pub_Error, errorCode);
      break;

    //[Controller] - Current outside temperature
    //Data Type: Byte Concat
    //Value: (Data[0] & Data[1]) / 100.0
    case 0x207:
      //Concat bytes 0 and 1 and divide the resulting INT by 100
      rawTemp = (Message.data[0] << 8) + Message.data[1];
      temp = rawTemp / 100.0;
      OutsideTemperatureSensor = temp;
      client.publish(pub_OutsideTemperature, String(temp).c_str());
      break;

    //Unknown
    case 0x208:
      break;

    //[Controller] - Gas Burner Flame Status
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x209:
      flame = Message.data[0];
      client.publish(pub_GasBurner, String(flame).c_str());
      break;

    //[HC] - [Controller] - HC Pump Operation
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x20A:
      hcPump = Message.data[0];
      client.publish(pub_HcPump, String(hcPump).c_str());
      break;

    //[DHW] - [Controller] - Hot Water Battery Operation
    //Data Type: Bit
    //Value: 1 = Enabled | 0 = Disabled
    case 0x20B:
      hwBatteryMode = Message.data[0];
      break;

    //[HC] - [Controller] - Current Seasonal Operation Mode (Set by Dial on the boiler panel)
    //Data Type: Bit
    //Value: 1 = Winter | 0 = Summer
    case 0x20C:
      hcSeason = Message.data[0];
      client.publish(pub_Season, String(hcSeason).c_str());
      break;

    //[HC] - [RC] - Heating Operating
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x250:
      hcActive = Message.data[0];
      client.publish(pub_HcOperation, String(hcActive).c_str());
      break;

    //[HC] - [RC] - Heating Power
    //Data Type: INT
    //Value: 0-255 = 0-100%
    case 0x251:
      hcHeatingPower = Message.data[0];
      break;

    //[HC] - [RC] - Setpoint Feed Temperature
    //Data Type: INT
    //Value: Value / 2.0
    //Set: Value as half-centigrade steps i.e. 35.5
    case 0x252:
      temp = Message.data[0] / 2.0;
      client.publish(pub_SetpointFeedTemperature, String(temp).c_str());
      break;

    //[DHW] - [RC] - Setpoint water temperature
    //Data Type: INT
    //Value: Data / 2.0
    //Set: //Set: Value as half-centigrade steps i.e. 45.5
    case 0x253:
      temp = Message.data[0] / 2.0;
      break;

    //[DHW] - [RC] - "Hot Water Now" (Warmwasser SOFORT in German)
    //Data Type: Bit
    //Value: 1 = Enabled | 0 = Disabled
    case 0x254:
      hwNow = Message.data[0];
      break;

    //[RC] - Date and Time
    //Data Type: Multibyte
    //Value: Data[0] = Day Of Week Number ( 1 = Monday)
    //       Data[1] = Hours (0-23)
    //       Data[2] = Minutes (0-59)
    //       Data[3] = Always '4' - Unknown meaning.
    case 0x256:
      curDayOfWeek = Message.data[0];
      curHours = Message.data[1];
      curMinutes = Message.data[2];
      break;

    //[DHW] - [RC] - Setpoint water temperature (Continuous-Flow Mode)
    //Data Type: INT
    //Value: Data / 2.0
    case 0x255:
      temp = Message.data[0] / 2.0;
      break;

    //[MC] - [Controller] - Mixed-Circuit Pump Operation
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x404:
      mcPump = Message.data[0];
      break;

    //[MC] - [RC] - Setpoint Mixed-Circuit Feed Temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x405:
      temp = Message.data[0] / 2.0;
      break;

    //[MC] - [RC] - Mixed-Circuit Economy Setting
    //Data Type: Bit
    //Value: 1 = On | 0 = Off
    case 0x407:
      mcEconomy = Message.data[0];
      break;

    //[MC] - [Controller] - Mixed-Circuit Current Feed Temperature
    //Data Type: INT
    //Value: Data / 2.0
    case 0x440:
      temp = Message.data[0] / 2.0;
      break;
    }
  }
}

// \brief Callback for MQTT subscribed topics
void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String s = String((char *)payload);
  if (!s)
  {
    return;
  }
  // Example for performing an action on topic receive.
  if (strcmp(topic, subTopic_Example) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    // Do something with 'i'
  }

  // Read Auxilary Outside Temperature
  if (strcmp(topic, subscription_AuxTemperature) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    WriteToConsoles("MQTT RCV: AuxTemp >> " + s + "\r\n");
    mqttAuxilaryTemperature = d;
  }

  //Read Heating Basepoint
  if (strcmp(topic, subscription_FeedBaseSetpoint) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    WriteToConsoles("MQTT RCV: BasePoint >> " + s + "\r\n");
    mqttBasepointTemperature = d;
  }

  //Read Heating Endpoint (Cut-Off)
  if (strcmp(topic, subscription_FeedCutOff) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    WriteToConsoles("MQTT RCV: Endpoint (Cut-Off) >> " + s + "\r\n");
    mqttEndpointTemperature = d;
  }

  //Read Heating Minimum Temperature (Anti-Freeze)
  if (strcmp(topic, subscription_FeedMinimum) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    WriteToConsoles("MQTT RCV: Feed Min (Anti-Freeze) >> " + s + "\r\n");
    mqttMinimumFeedTemperature = d;
  }

  //Read Ambient Temperature
  if (strcmp(topic, subscription_AmbientTemperature) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Ambient >> " + s + "\r\n");
    }
    mqttAmbientTemperature = d;
  }

  //Read Target Ambient Temperature
  if (strcmp(topic, subscription_TargetAmbientTemperature) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Ambient Target >> " + s + "\r\n");
    }
    mqttTargetAmbientTemperature = d;
  }

  //Read Heating on|off
  if (strcmp(topic, subscription_OnOff) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    WriteToConsoles("MQTT RCV: On/Off >> " + s + "\r\n");
    mqttHeatingSwitch = (i == 1 ? true : false);
    //Write value to the "control" level
    hcActive = mqttHeatingSwitch;
  }

  //Read Boost on|off
  if (strcmp(topic, subscription_OnDemandBoost) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Boost >> " + s + "\r\n");
    }
    mqttBoost = (i == 1 ? true : false);
    //Start Immediately.
    SetFeedTemperature();
  }

  //Read Boost Duration
  if (strcmp(topic, subscription_OnDemandBoostDuration) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Boost Duration >> " + s + "\r\n");
    }
    mqttBoostDuration = i;
  }

  //Read FastHeatup on|off
  if (strcmp(topic, subscription_FastHeatup) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Fast Heatup >> " + s + "\r\n");
    }
    mqttFastHeatup = (i == 1 ? true : false);
    //Start Immediately.
    SetFeedTemperature();
    //Set reference Temperature
    referenceAmbientTemperature = mqttAmbientTemperature;
  }
}

void connectWifi()
{
  //(re)connect WiFi
  if (!WiFi.isConnected())
  {
    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // Workaround: makes setHostname() work
    WiFi.setHostname(hostName);
    Serial.println("WiFi not connected. Reconnecting...");
    WiFi.begin(ssid, pass);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }
  }

  //Sync time
  SyncTimeIfRequired();

  unsigned long currentMillis = millis();
  //Print out WiFi Status
  if (currentMillis - wifiConnectMillis >= wifiRetryInterval)
  {
    wifiConnectMillis = currentMillis;
    if (WiFi.isConnected())
    {
      Serial.println("-------------------------------");
      Serial.println("Wifi Connected");
      Serial.print("SSID:\t");
      Serial.println(WiFi.SSID());
      Serial.print("IP Address:\t");
      Serial.println(WiFi.localIP());
      Serial.print("Mask:\t\t");
      Serial.println(WiFi.subnetMask());
      Serial.print("Gateway:\t");
      Serial.println(WiFi.gatewayIP());
      Serial.print("RSSI:\t\t");
      Serial.println(WiFi.RSSI());

      myTZ.setLocation(F("Europe/Berlin"));
      Serial.printf("Time: [%s]\r\n", myTZ.dateTime().c_str());
    }
  }
}
// \brief Enables OTA Updates
void ota()
{
  ArduinoOTA
      .onStart([]() {
        otaRunning = true;
        //End all "addons"
        TelnetServer.end();
        can.end();
        client.disconnect();

        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        otaRunning = false;
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();
}

// \brief (Re)connect to MQTT broker
void reconnectMqtt()
{
  if (!WiFi.isConnected())
  {
    Serial.println("Can't connect to MQTT broker. [No Network]");
    return;
  }

  // Loop until we're reconnected
  while (!client.connected())
  {

    Serial.print("Attempting MQTT connection...");

    String clientId = generateClientId();
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUsername, mqttPassword))
    {
      Serial.println("connected");

      // ... and resubscribe to topics
      client.subscribe(subTopic_Example);
      client.subscribe(subscription_AuxTemperature);
      client.subscribe(subscription_AmbientTemperature);
      client.subscribe(subscription_TargetAmbientTemperature);
      client.subscribe(subscription_FeedBaseSetpoint);
      client.subscribe(subscription_FeedCutOff);
      client.subscribe(subscription_FeedSetpoint);
      client.subscribe(subscription_FeedMinimum);
      client.subscribe(subscription_OnOff);
      client.subscribe(subscription_OnDemandBoost);
      client.subscribe(subscription_OnDemandBoostDuration);
      client.subscribe(subscription_FastHeatup);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//Returns a client id for MQTT communication
String generateClientId()
{
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  // Create client ID using MAC address
  String clientId = "ESP-";
  clientId += macAddress;

  return clientId;
}

//Checks for new Telnet connections
void CheckForConnections()
{
  //Check if we're having a new client connection
  if (TelnetServer.hasClient())
  {
    //Check if we already have a client
    if (TelnetRemoteClient.connected())
    {
      Serial.println("Telnet Connection rejected");
      //Reject connection.
      TelnetServer.available().stop();
    }
    else
    {
      Serial.println("Telnet Connection accepted");
      //Accept
      TelnetRemoteClient = TelnetServer.available();
      TelnetRemoteClient.println("——————————————————————————");
      TelnetRemoteClient.printf("You are connected to: %s (%s)\r\n", hostName, WiFi.localIP().toString().c_str());
      TelnetRemoteClient.println("——————————————————————————");
    }
  }
}

//Write messages to both Serial and Telnet Clients
void WriteToConsoles(String message)
{
  Serial.print(message);
  //Print Message only if a client is connected and there is no data in the receive buffer.
  if (TelnetRemoteClient.connected() && TelnetRemoteClient.available() == 0)
  {
    TelnetRemoteClient.print(message);
  }
}

//Read commands from Telnet clients.
void ReadFromTelnet()
{
  //Very basic implementation. Does the job but isn't perfect...
  if (TelnetRemoteClient.connected() && TelnetRemoteClient.available())
  {
    String command = TelnetRemoteClient.readStringUntil('\r');
    command.trim();
    command.toLowerCase();

    //Do nothing on empty command.
    if (command.length() == 0)
    {
      return;
    }

    if (strcmp(command.c_str(), "reboot") == 0)
    {
      TelnetRemoteClient.println("Reboot in 5 seconds...");
      delay(5000);
      ESP.restart();
      return;
    }
  }
}

//-- Takes the parameters and calculates the desired feed temperature.
//   This calculation takes in the desired base temperature at which the heating
//   should perform 100% of the reported maximum feed temperature and when it should perform at the minimum temperature.
//   The calculation maps endpoint and basepoint to minimum temperature and maximum feed temperature
//   The original controller takes a different approach: It lets you decide which temperature should be set when the outside temperature is -15°C or -20°C for some models
//   The calculation for the original controller is: map 25° and -15° to Off-Temperature and maximum temperature.
double CalculateFeedTemperature()
{
  char printbuf[255];
  //Map the current ambient temperature to the desired feed temperature:
  //        Ambient Temperature input, Endpoint i.e. 25°, Base Point i.e. -15°, Minimum Temperature at 25° i.e. 10°, Maximum Temperature at -15° i.e. maximum feed temperature the heating is capable of.
  if (!hcActive)
  {
    if (isOnFallback)
    {
      return fallbackMinimumFeedTemperature;
    }
    else
    {
      return mqttMinimumFeedTemperature;
    }
  }

  //The map() function will freeze the ESP if the values are the same.
  //  We will return the base value instead of calculating.
  if (mqttBasepointTemperature == mqttEndpointTemperature)
  {
    return mqttBasepointTemperature;
  }

  //Return max feed temperature when boost is requested.
  if (mqttBoost)
  {
    return hcMaxFeed;
  }

  //Map Value
  double linearTemp = map_Generic(OutsideTemperatureSensor, mqttEndpointTemperature, mqttBasepointTemperature, mqttMinimumFeedTemperature, hcMaxFeed);
  //Add Adaption
  linearTemp += mqttFeedAdaption;
  //Round value to half steps
  double halfRounded = llround(linearTemp * 2) / 2.0;

  //Return max feed temperature if fast heatup is on and the ambient temperature hasn't been reached yet.
  if (mqttFastHeatup)
  {
    //Target Temperature hasn't been reached
    if (mqttAmbientTemperature < mqttTargetAmbientTemperature)
    {

      //Let's tune this so the heating hasn't to work that hard all the way
      //  We can take the temperature difference as reference and map the max feed temperature accordingly
      //First we calculate the difference between desired temperature (target) and actual value
      double tempDiff = mqttTargetAmbientTemperature - mqttAmbientTemperature;

      //Sanity check: max feed equals calculated
      if (hcMaxFeed == linearTemp)
      {
        //Bail out.
        return hcMaxFeed;
      }

      //Note: We don't have to check the target and current ambient temperature as this case is already handled by the initial comparison

      //Now we map the difference, which is decreasing over time, to the fixed range between reference (=starting point) and target ambient
      //  We map it according to the maximum available feed temperature and the currently calculated temperature.
      //Previous checks will prevent any divisions by zero that would otherwise stop the MC from operating
      double fhTemp = map_Generic(tempDiff, 0, tempDiff, linearTemp, hcMaxFeed);
      //Expected Values:
      //Room Temperature is 17°C
      //Target is 21°C
      //Difference is 4°C
      //Calculated Feed is 50°
      //Max is 75°C
      //Result: ( ( 4   -   0 )   ×   ( 75   -   50 ) )   ÷   ( 4   -   0 )   +   50 = 75

      //Room Temperature is 21°C
      //Target is 21°C
      //Difference is 0°C
      //Calculated Feed is 50°
      //Max is 75°C
      //Result: ( ( 0   -   0 )   ×   ( 75   -   50 ) )   ÷   ( 0   -   0 )   +   50 = 50
      //Half-Step-Round
      double hrFhTemp = llround(fhTemp * 2) / 2.0;

      if (Debug)
      {
        sprintf(printbuf, "DEBUG SET TEMP: Fast Heatup is active. Current: %.2f Target: %.2f Setpoint is %.2f \r\n", mqttAmbientTemperature, mqttTargetAmbientTemperature, fhTemp);
        String message(printbuf);
        WriteToConsoles(message);
      }

      //Return Result.
      return hrFhTemp;
    }
    //If we reached the goal, set it to false again so it won't trigger again when the temperature drops
    else
    {
      mqttFastHeatup = false;
    }
  }

  if (Debug)
  {

    sprintf(printbuf, "DEBUG MAP VALUE: %.2f >> from %.2f to %.2f to %.2f and %.2f >> %.2f >> Half-Step Round: %.2f", OutsideTemperatureSensor, mqttEndpointTemperature, mqttBasepointTemperature, mqttMinimumFeedTemperature, hcMaxFeed, linearTemp, halfRounded);
    String message(printbuf);
    WriteToConsoles(message + "\r\n");
  }
  return halfRounded;
}

//Converts the value to its half-step representation (= value times two)
int ConvertFeedTemperature(double temperature)
{
  return temperature * 2;
}

//Sync using NTP, if clock is off
void SyncTimeIfRequired()
{
  //Sync Time if required
  timeStatus_t timeStat = timeStatus();
  if (timeStat != timeSet)
  {
    waitForSync();
  }
}

//Returns TRUE if the clock is on point and false if it requires calibration
bool TimeIsSynced()
{
  return timeStatus() == timeSet;
}