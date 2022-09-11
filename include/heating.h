#ifndef _HEATING_H
#define _HEATING_H

#include <Arduino.h>
#include <main.h>


//——————————————————————————————————————————————————————————————————————————————
//  Structs
//——————————————————————————————————————————————————————————————————————————————

//Represents a very basic structure of a time scheduler entry. It is used by the fallback mechanism.
typedef struct HeatingScheduleEntry_
{
  int StartHour;
  int StartMinute;
  int DayOfWeek;
  bool heat;
} HeatingScheduleEntry;

//——————————————————————————————————————————————————————————————————————————————
//  Constants for heating control
//——————————————————————————————————————————————————————————————————————————————

//Basepoint for linear temperature calculation
extern const int calcHeatingBasepoint;

//Endpoint for linear temperature calculation
extern const int calcHeatingEndpoint;

//Minimum Feed Temperature. This is the base value for calculations. Defaults to 10° on most devices.
extern const int calcTriggerAntiFreeze;

//-- Heating Scheduler. Fallback values for when the MQTT broker isn't available
extern HeatingScheduleEntry fallbackStartEntry;
extern HeatingScheduleEntry fallbackEndEntry;

//-- Temperature
extern double temp;

//-- Heating Circuit: Max. Feed Temperature (From Controller)
extern double hcMaxFeed;

//-- Heating Circuit: Current Feed Temperature (From Controller)
extern double hcCurrentFeed;

//-- Heating Circuit: Feed Temperature Setpoint (Control Value)
extern double hcFeedSetpoint;

//-- Heating Controller: Current Temperature on the Outside (From Controller)
extern double OutsideTemperatureSensor;

//-- Heating Controller: Flame lit (From Controller)
extern bool flame;

//-- Heating Controller: Status. 0x0 = Operational. Error Flags vary between models!
extern uint8_t status;

//-- Heating Circuit: Circulation Pump on|off
extern bool hcPump;

//-- Hot Water Battery Mode Status
extern bool hwBatteryMode;

//-- Heating Seasonal Mode. True = Summer | False = Winter
extern bool hcSeason;

//-- Mixed-Circuit Pump Status
extern bool mcPump;

//-- Mixed-Circuit Economy Setting
extern bool mcEconomy;

//-- Current Day of the week as configured by the CC
extern int curDayOfWeek;

//-- Current Hour component
extern int curHours;

//-- Current Minutes component
extern int curMinutes;

//-- Heating active / operational
extern bool hcActive;

//-- Analog value of heating power
extern int hcHeatingPower;

//-- Hot Water "now" setting.
extern bool hwNow;

//-- Max. possible water temperature -or- target temperature when running in heating battery mode
extern double hcMaxWaterTemperature;

//-- Current Water Temperature
extern double hcCurrentWaterTemperature;

//-- Fallback Values

//Basepoint Temperature
extern double fallbackBasepointTemperature;
//Endpoint Temperature
extern double fallbackEndpointTemperature;
//Ambient Temperature
extern double fallbackAmbientTemperature;
//Minimum ("Anti Freeze") Temperature.
extern double fallbackMinimumFeedTemperature;
//Enforces the fallback values when set to TRUE
extern bool isOnFallback;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern double CalculateFeedTemperature();
extern int ConvertFeedTemperature(double temperature);

extern void SetFeedTemperature();

#endif