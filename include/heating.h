#ifndef _HEATING_H
#define _HEATING_H

#include <Arduino.h>
#include <main.h>

//——————————————————————————————————————————————————————————————————————————————
//  Structs
//——————————————————————————————————————————————————————————————————————————————

// Represents a very basic structure of a time scheduler entry. It is used by the fallback mechanism.
typedef struct HeatingScheduleEntry_
{
  int StartHour;
  int StartMinute;
  int DayOfWeek;
  bool heat;
} HeatingScheduleEntry;

// @brief Holds the current temperatures and settings
struct CeraValues
{

  //-- Heating Circuit Values
  struct Heating_
  {
    //-- Max. Feed Temperature
    double FeedMaximum = 75.00F;
    //-- Current Feed Temperature
    double FeedCurrent = 0.00F;
    //-- Feed Temperature Setpoint
    double FeedSetpoint = 40.00F;
    //-- Minimum Feed Temperature as reported by the boiler
    double FeedMinimum = 10.0F;
    //-- Max. possible water temperature -or- target temperature when running in heating Buffer mode
    double BufferWaterTemperatureMaximum = 0.0F;
    //-- Current Water Temperature (Buffer Mode)
    double BufferWaterTemperatureCurrent = 0.0F;
    //-- Circulation Pump Active
    bool PumpActive = false;
    //-- Seasonal Mode: True = Summer | False = Winter
    bool Season = false;
    //-- Heating active / operational
    bool Active = true;
    //-- Analog value of heating power
    int HeatingPower = 0;
    //-- Economy Setting
    bool Economy = false;
  } Heating;
  //-- General Values, i.e. Flame, Error, ...
  struct General_
  {
    //-- Flame status as reported by the gas burner
    bool FlameLit = false;
    //-- Readings of the temperature sensor on the outside
    double OutsideTemperature = 0.00F;
    //-- Errorcode, if any, defaults to 0x0 as "operational"
    uint8_t Error = 0x000;
  } General;
  //-- Domestic Hot Water (DHW) Values
  struct HotWater_
  {
    //-- Setpoint (Target Temperature) for DHW
    int SetPoint = 40.0F;
    //-- The currently reported temperature of the DHW circuit
    double TemperatureCurrent = 0.00F;
    //-- Whether this installation utilizes a buffer(or battery)
    bool BufferMode = false;
    //-- Hot Water "now" setting (means: Instant heatup but is referred to as "now" on TAxxx and the manual).
    bool Now = false;
    //-- Continous Flow Setpoint
    double ContinousFlowSetpoint = 0.00F;
    //-- Maximum Temperature
    double MaximumTemperature =0.00F;
  } Hotwater;
  //-- Values for mixed circuit installations
  struct MixedCircuit_
  {
    //-- Mixed-Circuit Pump Status
    bool PumpActive = false;
    //-- Mixed-Circuit Economy Setting
    bool Economy = false;
    //-- Feed Setpoint
    double FeedSetpoint = 0.00F;
    //-- Current Feed Temperature
    double FeedCurrent = 0.00F;
  } MixedCircuit;

  //-- Fallback Values
  //TODO: Should be configurable using configuration.json
  struct FallBack_
  {
    //-- Basepoint Temperature
    double BasepointTemperature = -10.0F;
    //-- Endpoint Temperature
    double EndpointTemperature = 31.0F;
    //-- Ambient Temperature
    double AmbientTemperature = 17.0F;
    //-- Minimum ("Anti Freeze") Temperature.
    double MinimumFeedTemperature = 10.0F;
    //-- Enforces the fallback values when set to TRUE
    bool isOnFallback = false;

    //-- Heating Scheduler. Fallback values for when the MQTT broker isn't available
    HeatingScheduleEntry fallbackStartEntry = {5, 30, 0, true};
    HeatingScheduleEntry fallbackEndEntry = {23, 30, 0, false};
  } Fallback;

  struct BaseValues_
  {
    //-- Basepoint for linear temperature calculation
    int calcHeatingBasepoint = -15;

    //-- Endpoint for linear temperature calculation
    int calcHeatingEndpoint = 21;

    //-- Minimum Feed Temperature. This is the base value for calculations. Defaults to 10° on most devices.
    int calcTriggerAntiFreeze = 10;
  } BaseValues;

  struct Time_
  {
    //-- Current Day of the week as configured by the CC
    int DayOfWeek = 0;

    //-- Current Hour component
    int Hours = 0;

    //-- Current Minutes component
    int Minutes = 0;
  } Time;

  struct Auxilary_
  {
    double *Temperatures;
    double FeedReturnTemperatureReference;
  } Auxilary;
};

extern CeraValues ceraValues;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern double CalculateFeedTemperature();
extern int ConvertFeedTemperature(double temperature);
extern void SetFeedTemperature();

#endif