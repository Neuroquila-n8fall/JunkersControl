#include <Arduino.h>
#include <arduino_secrets.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ACAN2515.h>
#include <eztime.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <templates.h>

//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

//Set this to true to override the RC (TR/TA). Use with caution. Will write
//  values on the bus and actively steers the heating when turned on.
//  Will also conflict with any existing Room/Remote Controller on the bus.
const bool Override = false;

//Set this to true to view debug info
const bool Debug = true;

//——————————————————————————————————————————————————————————————————————————————
//  Structs
//——————————————————————————————————————————————————————————————————————————————

struct HeatingScheduleEntry
{
  int StartHour;
  int StartMinute;
  int DayOfWeek;
  bool heat;
};

//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 SPI Pin Assignment
//——————————————————————————————————————————————————————————————————————————————

static const byte MCP2515_SCK = 18;  // SCK input of MCP2515
static const byte MCP2515_MOSI = 23; // SDI input of MCP2515
static const byte MCP2515_MISO = 19; // SDO output of MCP2515
static const byte MCP2515_CS = 5;    // CS input of MCP2515 (adapt to your design)
static const byte MCP2515_INT = 17;  // INT output of MCP2515 (adapt to your design)

//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 Driver object
//——————————————————————————————————————————————————————————————————————————————

ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);

//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 Quartz: adapt to your design
//——————————————————————————————————————————————————————————————————————————————

static const uint32_t QUARTZ_FREQUENCY = 16UL * 1000UL * 1000UL; // 16 MHz

//——————————————————————————————————————————————————————————————————————————————
//  WiFi Settings
const char *ssid = SECRET_SSID;
const char *pass = SECRET_PASS;
const char hostName[] = "FXHEATCTRL01";
//-- WiFi Check interval for status output
const int wifiRetryInterval = 30000;
//-- Wifi Client object
WiFiClient espClient;

//——————————————————————————————————————————————————————————————————————————————
//  NTP Time Object
//——————————————————————————————————————————————————————————————————————————————
Timezone myTZ;

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Topics
//——————————————————————————————————————————————————————————————————————————————

//-- Subscriptions

//Example Topic
const char subTopic_Example[] = "heizung/control/something";

//Heating ON|OFF Control
const char subscription_OnOff[] = "heizung/control/operation";

//Heating Setpoint Feed
const char subscription_FeedSetpoint[] = "heizung/control/sollvorlauf";

//Heating Adaption: Feed Temperature @ -15°C (ZSR24-6)
const char subscription_FeedBaseSetpoint[] = "heizung/parameters/fusspunkt";

//Heating Adaption: Feed Cut-Off Temperature based on outside temperature reading
const char subscription_FeedCutOff[] = "heizung/parameters/endpunkt";

//Heating Adaption: Minimum Temperature of Feed (Anti-Freeze)
const char subscription_FeedMinimum[] = "heizung/parameters/minimum";

//Secondary Outside Temperature Source
const char subscription_AuxTemperature[] = "umwelt/temperaturen/aussen";

//Ambient Temperature Source
const char subscription_AmbientTemperature[] = "heizung/parameters/ambient";

//-- Published Topics

//Maximum Feed Temperature
const char pub_HcMaxFeedTemperature[] = "heizung/temperaturen/maxvorlauf";

//Current Feed Temperature
const char pub_CurFeedTemperature[] = "heizung/temperaturen/aktvorlauf";

//Setpoint of feed Temperature
const char pub_SetpointFeedTemperature[] = "heizung/temperaturen/sollvorlauf";

//The Temperature on the Outside
const char pub_OutsideTemperature[] = "heizung/temperaturen/aussen";

//Flame status
const char pub_GasBurner[] = "heizung/status/brenner";

//Heating Circuit Pump Status
const char pub_HcPump[] = "heizung/status/pumpe";

//General Error Flag
const char pub_Error[] = "heizung/status/fehler";

//Seasonal Operation Flag
const char pub_Season[] = "heizung/status/betriebsmodus";

//Heating Operating
const char pub_HcOperation[] = "heizung/status/heizbetrieb";

//-- MQTT Endpoint
const char *mqttServer = mqttSERVER;
const char *mqttUsername = mqttUSERNAME;
const char *mqttPassword = mqttPASSWORD;

//-- MQTT Client (uses Wifi Client)
PubSubClient client(espClient);

//——————————————————————————————————————————————————————————————————————————————
//  Constants for heating control
//——————————————————————————————————————————————————————————————————————————————

//Basepoint for linear temperature calculation
const int calcHeatingBasepoint = -15;

//Endpoint for linear temperature calculation
const int calcHeatingEndpoint = 21;

//Switch-On Temperature Threshold. The Heating should start to heat if the ambient temperature is lower than this value.
const int calcTriggerAntiFreeze = 10;

//——————————————————————————————————————————————————————————————————————————————
//  Server for remote console. We're using the telnet port here
//——————————————————————————————————————————————————————————————————————————————

const uint TelnetServerPort = 23;
WiFiServer TelnetServer(TelnetServerPort);
WiFiClient TelnetRemoteClient;

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

//-- OTA Flag
bool otaRunning = false;

//-- WiFi Status Timer Variable
unsigned long wifiConnectMillis = 0L;

//-- One-Second Interval Timer Variable
unsigned long oneSecondTimer = 0L;

//-- Five-Second Interval Timer Variable
unsigned long fiveSecondTimer = 0L;

//-- Thirty-Second Interval Timer Variable
unsigned long thirtySecondTimer = 0L;

//-- Step-Counter
int currentStep = 0;

//-- Temperature
double temp = 0.00F;

//-- Heating Circuit: Max. Feed Temperature (From Controller)
double hcMaxFeed = 70.00F;

//-- Heating Circuit: Current Feed Temperature (From Controller)
double HkAktVorlauf = 0.00F;

//-- Heating Circuit: Feed Temperature Setpoint (Control Value)
double HkSollVorlauf = 40.00F;

//-- Heating Controller: Current Temperature on the Outside (From Controller)
double OutsideTemperatureSensor = 0.00F;

//-- Heating Controller: Flame lit (From Controller)
bool flame = false;

//-- Heating Controller: Status. 0x0 = Operational. Error Flags vary between models!
uint8_t status = 0x00;

//-- Heating Circuit: Circulation Pump on|off
bool hcPump = false;

//-- Hot Water Battery Mode Status
bool hwBatteryMode = false;

//-- Heating Seasonal Mode
bool hcSeason = false;

//-- Mixed-Circuit Pump Status
bool mcPump = false;

//-- Mixed-Circuit Economy Setting
bool mcEconomy = false;

//-- Current Day of the week as configured by the CC
int curDayOfWeek = 0;

//-- Current Hour component
int curHours = 0;

//-- Current Minutes component
int curMinutes = 0;

//-- Heating active / operational
bool hcActive = false;

//-- Analog value of heating power
int hcHeatingPower = 0;

//-- Hot Water "now" setting.
bool hwNow = false;

//-- Variables set by MQTT subscriptions with factory defaults at startup

//Feed Temperature
double mqttFeedTemperatureSetpoint = 50.0F;
//Basepoint Temperature
double mqttBasepointTemperature = -10.0F;
//Endpoint Temperature
double mqttEndpointTemperature = 31.0F;
//Ambient Temperature
double mqttAmbientTemperature = 0.0F;
//Minimum ("Anti Freeze") Temperature.
double mqttMinimumFeedTemperature = 10.0F;
//Wether the heating should be off or not
bool mqttHeatingSwitch = true;
//Auxilary Temperature
double mqttAuxilaryTemperature = 0.0F;

//-- Fallback Values

//Basepoint Temperature
double fallbackBasepointTemperature = -10.0F;
//Endpoint Temperature
double fallbackEndpointTemperature = 31.0F;
//Ambient Temperature
double fallbackAmbientTemperature = 0.0F;
//Minimum ("Anti Freeze") Temperature.
double fallbackMinimumFeedTemperature = 10.0F;
//Enforces the fallback values when set to TRUE
bool isOnFallback = false;

//-- Heating Scheduler. Fallback values for when the MQTT broker isn't available
HeatingScheduleEntry fallbackStartEntry = {5, 30, 0, true};
HeatingScheduleEntry fallbackEndEntry = {23, 30, 0, false};

//——————————————————————————————————————————————————————————————————————————————
//  Function Definition. Required by Platform.io Compiler.
//——————————————————————————————————————————————————————————————————————————————

void connectWifi();
void ota();
void reconnectMqtt();
void setupCan();
void processCan();
void callback(char *topic, byte *payload, unsigned int length);
String generateClientId();
void CheckForConnections();
void WriteToConsoles(String message);
void ReadFromTelnet();

void SyncTimeIfRequired();
bool TimeIsSynced();

double CalculateFeedTemperature();
int ConvertFeedTemperature(double temperature);

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

  unsigned long currentMillis = millis();
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

  //Actions performed every second
  if (currentMillis - oneSecondTimer >= 1000)
  {
    oneSecondTimer = currentMillis;
    //Ensure that we are connected to MQTT
    reconnectMqtt();

    //Send desired Values to the heating controller
    //Note that it cannot perform unrealistic actions.
    //The built-in controller of the heating will always take care of staying well within the specs
    //We can only "suggest" to set to a certain temperature or switching off the pump(s)

    //I have "borrowed" the concept of a step-chain from PLC programming since it appears
    //  to have been incoorporated into the controller as well because values arrive in
    //  intervals of approximately 1 second.

    CANMessage msg;
    char printbuf[255];
    double feedTemperature = 0.0F;
    int feedSetpoint = 0;
    switch (currentStep)
    {
    case 0:
      //Fetch the correct value depending on the current operational state of the heating.
      if (hcActive)
      {
        //Send Setpoint
        feedTemperature = CalculateFeedTemperature();       
        feedSetpoint = ConvertFeedTemperature(feedTemperature);
      }
      else
      {
        //Send Minimum Setpoint

        if (isOnFallback)
        {
          //Use fallback value when on fallback mode
          feedSetpoint = ConvertFeedTemperature(fallbackMinimumFeedTemperature);
        }
        else
        {
          //Use commanded value when connected
          feedSetpoint = ConvertFeedTemperature(mqttMinimumFeedTemperature);
        }
      }

      msg.id = 0x252;
      msg.data[0] = feedSetpoint;
      if (Debug)
      {
        sprintf(printbuf, "DEBUG STEP CHAIN #%i: Heating is %s, Fallback is %s, Feed Setpoint is %f, INT representation (half steps) is %i",currentStep, hcActive ? "ON" : "OFF", isOnFallback ? "YES" : "NO" , feedTemperature, feedSetpoint);
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
      }

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
      //Switch heating on|off
      msg.id = 0x250;
      msg.data[0] = mqttHeatingSwitch;
      if (Debug)
      {
        sprintf(printbuf, "DEBUG STEP CHAIN #%i: Heating Operation: %d", currentStep, hcActive);
        String message(printbuf);
        WriteToConsoles(message + "\r\n");
      }
      break;

    case 3:
      break;

    case 4:
      break;

    case 5:
      break;

    default:
      //If we reach any undefined number inside the chain, reset to zero
      currentStep = 0;
      return; // important!
    }

    //Send message if not empty and override is true.
    if (msg.id != 0 && Override)
    {
      can.tryToSend(msg);
    }

    //Increase counter
    currentStep++;
  }

  //Actions performed every five seconds
  if (currentMillis - fiveSecondTimer >= 5000)
  {
    fiveSecondTimer = currentMillis;
  }

  //Actions performed every thirty seconds
  if (currentMillis - thirtySecondTimer >= 30000)
  {
    thirtySecondTimer = currentMillis;

    // Run on fallback values when the connection to the server has been lost.
    if (TimeIsSynced() && !client.connected())
    {
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
      client.publish(pub_Error, String(status).c_str());
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
      client.subscribe(subscription_FeedBaseSetpoint);
      client.subscribe(subscription_FeedCutOff);
      client.subscribe(subscription_FeedSetpoint);
      client.subscribe(subscription_FeedMinimum);
      client.subscribe(subscription_OnOff);
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

//-- Takes the parameters and calculates the desired feed temperature using the outside temperature sensor
double CalculateFeedTemperature()
{
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

  double linearTemp = map_Generic(OutsideTemperatureSensor, mqttEndpointTemperature, mqttBasepointTemperature, mqttMinimumFeedTemperature, hcMaxFeed);

  double halfRounded = llround(linearTemp * 2) / 2.0;
  if (Debug)
  {
    char printbuf[255];
    sprintf(printbuf, "DEBUG MAP VALUE: %.2f >> from %.2f to %.2f to %.2f and %.2f >> %.2f >> Half-Step Round: %.2f", OutsideTemperatureSensor, mqttEndpointTemperature, mqttBasepointTemperature, mqttMinimumFeedTemperature, hcMaxFeed, linearTemp, halfRounded);
    String message(printbuf);
    WriteToConsoles(message + "\r\n");
  }
  return halfRounded;
}

int ConvertFeedTemperature(double temperature)
{
  return temperature * 2;
}

void SyncTimeIfRequired()
{
  //Sync Time if required
  timeStatus_t timeStat = timeStatus();
  if (timeStat != timeSet)
  {
    waitForSync();
  }
}

bool TimeIsSynced()
{
  return timeStatus() == timeSet;
}