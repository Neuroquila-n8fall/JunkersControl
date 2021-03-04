#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>

//——————————————————————————————————————————————————————————————————————————————
//  Structs
//——————————————————————————————————————————————————————————————————————————————

//Represents a very basic structure of a time scheduler entry. It is used by the fallback mechanism.
struct HeatingScheduleEntry
{
  int StartHour;
  int StartMinute;
  int DayOfWeek;
  bool heat;
};

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

//-- Last Controller Message timer
unsigned long controllerMessageTimer = 0L;

//-- Step-Counter
int currentStep = 0;

//-- Temperature
double temp = 0.00F;

//-- Heating Circuit: Max. Feed Temperature (From Controller)
double hcMaxFeed = 75.00F;

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

//-- Heating Seasonal Mode. True = Summer | False = Winter
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
bool hcActive = true;

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
double mqttAmbientTemperature = 17.0F;
//Target Ambient Temperature to reach
double mqttTargetAmbientTemperature = 21.5F;
//Feed Temperature Adaption Value. Increases or decreases the target feed temperature
double mqttFeedAdaption = 0.00F;
//Minimum ("Anti Freeze") Temperature.
double mqttMinimumFeedTemperature = 10.0F;
//Whether the heating should be off or not
bool mqttHeatingSwitch = true;
//Auxilary Temperature
double mqttAuxilaryTemperature = 0.0F;
//Boost will max the feed temperature for mqttBoostTime seconds
bool mqttBoost = false;
//Boost Duration (Seconds)
int mqttBoostDuration = 300;
//Countdown variable for boost control.
int boostTimeCountdown = mqttBoostDuration;
//Fast Heatup. This will max out the feed temperature for a prolongued time until mqttTargetAmbientTemperature has been reached. Setting this to false will return to normal mode in any case.
bool mqttFastHeatup = false;
//Stored Ambient Temperatur as reference. Reset everytime the Fastheatup flag is set
double referenceAmbientTemperature = 17.0F;

//-- Fallback Values

//Basepoint Temperature
double fallbackBasepointTemperature = -10.0F;
//Endpoint Temperature
double fallbackEndpointTemperature = 31.0F;
//Ambient Temperature
double fallbackAmbientTemperature = 17.0F;
//Minimum ("Anti Freeze") Temperature.
double fallbackMinimumFeedTemperature = 10.0F;
//Enforces the fallback values when set to TRUE
bool isOnFallback = false;

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

void SetFeedTemperature();

void ReadAndSendTemperatures();

#endif