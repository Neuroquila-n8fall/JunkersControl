#ifndef _MQTT_H
#define _MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <wifi_config.h>

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Client (uses Wifi Client)
//——————————————————————————————————————————————————————————————————————————————
extern PubSubClient client;

struct CommandedValues
{
    struct Heating_
    {
        //-- Feed Temperature Setpoint
        double FeedSetpoint = 40.00F;
        //-- Calculated Feed Temperature
        double CalculatedFeedSetpoint = 40.0F;
        //-- Minimum Feed Temperature
        double FeedMinimum = 10.0F;
        // Basepoint Temperature
        double BasepointTemperature = -10.0F;
        // Endpoint Temperature
        double EndpointTemperature = 31.0F;
        // Ambient Temperature
        double AmbientTemperature = 17.0F;
        // FastHeatup will keep the heating on maximum feed setpoint until this temperature has been reached.
        double TargetAmbientTemperature = 21.5F;
        // Feed Temperature Adaption Value. Increases or decreases the target feed temperature
        double FeedAdaption = 0.00F;
        // Minimum ("Anti Freeze") Temperature.
        double MinimumFeedTemperature = 10.0F;
        // Whether the heating should be off or not
        bool Active = true;
        // Auxilary Temperature
        double AuxilaryTemperature = 0.00F;
        // Boost will max the feed temperature for BoostTime seconds
        bool Boost = false;
        // Boost Duration (Seconds)
        int BoostDuration = 300;
        // Countdown variable for boost control.
        int BoostTimeCountdown = BoostDuration;
        // Fast Heatup. This will max out the feed temperature for a prolongued time until TargetAmbientTemperature has been reached. Setting this to false will return to normal mode in any case.
        bool FastHeatup = false;
        // Stored Ambient Temperatur as reference. Reset everytime the Fastheatup flag is set
        double ReferenceAmbientTemperature = 17.0F;
        // Dynamic Adaption Flag
        bool DynamicAdaption = false;
        // Take the value from commandedValues.Heating.FeedSetpoint instead of doing built-in calculations.
        bool OverrideSetpoint = false;
        // Enable scaling based upon valve opening
        bool ValveScaling = false;
        // Maximum valve opening. 80% for Homematic eTRV-2
        int MaxValveOpening = 80;
        // The value of the valve that is most open
        int ValveOpening = 0;
    } Heating;

    struct HotWater_
    {
        //-- Setpoint (Target Temperature) for DHW
        int SetPoint = 40.0F;
    } HotWater;

};

extern CommandedValues commandedValues;

enum LogLevel
{
    Error,
    Warn,
    Info,
    Debug,
    Verbose
};

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void reconnectMqtt();
extern String generateClientId();
extern void setupMqttClient();
extern void callback(char *topic, byte *payload, unsigned int length);
extern void PublishStatus();
extern void PublishHeatingTemperaturesAndStatus();
extern void PublishWaterTemperatures();
extern void PublishAuxilaryTemperatures();
extern void ShowActivityLed();
extern String boolToString(bool src);
extern void PublishLog(const char *msg, const char *func, LogLevel level);

#endif