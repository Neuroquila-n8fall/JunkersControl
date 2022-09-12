#ifndef _MQTT_H
#define _MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <arduino_secrets.h>
#include <wifi_config.h>

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Client (uses Wifi Client)
//——————————————————————————————————————————————————————————————————————————————
extern PubSubClient client;

//-- MQTT Endpoint
extern const char *mqttServer;
extern const char *mqttUsername;
extern const char *mqttPassword;

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Topics
//——————————————————————————————————————————————————————————————————————————————

//-- Subscriptions

//Example Topic
extern const char subTopic_Example[];

//Heating ON|OFF Control
extern const char subscription_OnOff[];

//Heating Setpoint Feed
extern const char subscription_FeedSetpoint[];

//Heating Adaption: The Outside temperature at which hcMaxFeed should be reached
extern const char subscription_FeedBaseSetpoint[];

//Heating Adaption: The Outside temperature at which the minimum temperature should be set
extern const char subscription_FeedCutOff[];

//Heating Adaption: Minimum Temperature of Feed (Anti-Freeze)
extern const char subscription_FeedMinimum[];

//Secondary Outside Temperature Source
extern const char subscription_AuxTemperature[];

//Ambient Temperature Source
extern const char subscription_AmbientTemperature[];

//Ambient Temperature Target
extern const char subscription_TargetAmbientTemperature[];

//On-Demand Boost.
extern const char subscription_OnDemandBoost[];

//On-Demand Boost: Time in seconds
extern const char subscription_OnDemandBoostDuration[];

//Fast-Heatup
extern const char subscription_FastHeatup[];

//Adaption
extern const char subscription_Adaption[];

//Valve Scaling
extern const char subscription_ValveScaling[];

//Valve Scaling Max Opening
extern const char subscription_ValveScalingMaxOpening[];

//Valve Scaling Current Top Opening Value
extern const char subscription_ValveScalingOpening[];

//Dynamic Adaption based on return temperature sensor and desired target room temperature
//  This enables a mode in which the feed temperature will be decreased when the return temperature is much higher than required
//  This is useful when the heating is much more capable than required.
//  Example: The return Temperature is 40°, the desired room temperature is 21°: The Adaption is -19° --> Feed setpoint is decreased by 19°
//  Another one: The Return temperature is 20°, the desired room temperature is 21°: The Adaption is +1° --> Feed setpoint increased by 1°
extern const char subscription_DynamicAdaption[];

extern const char subscription_OverrideSetpoint[];

//-- Published Topics

//Maximum Feed Temperature
extern const char pub_HcMaxFeedTemperature[];

//Current Feed Temperature
extern const char pub_CurFeedTemperature[];

//Setpoint of feed Temperature
extern const char pub_SetpointFeedTemperature[];

//The Temperature on the Outside
extern const char pub_OutsideTemperature[];

//Flame status
extern const char pub_GasBurner[];

//Heating Circuit Pump Status
extern const char pub_HcPump[];

//General Error Flag
extern const char pub_Error[];

//Seasonal Operation Flag
extern const char pub_Season[];

//Heating Operating
extern const char pub_HcOperation[];

//Boost
extern const char pub_Boost[];

//Fast Heatup
extern const char pub_Fastheatup[];

//-- Topics for Temperature Sensor Readings

//Feed Temperature Topic
extern const char pub_SensorFeed[];

//Return Temperature Topic
extern const char pub_SensorReturn[];

//Exhaust Temperature Topic
extern const char pub_SensorExhaust[];

//Ambient Temperature Topic
extern const char pub_SensorAmbient[];

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

//-- Variables set by MQTT subscriptions with factory defaults at startup

//Feed Temperature
extern double mqttFeedTemperatureSetpoint;
//Basepoint Temperature
extern double mqttBasepointTemperature;
//Endpoint Temperature
extern double mqttEndpointTemperature;
//Ambient Temperature
extern double mqttAmbientTemperature;
//Target Ambient Temperature to reach
extern double mqttTargetAmbientTemperature;
//Feed Temperature Adaption Value. Increases or decreases the target feed temperature
extern double mqttFeedAdaption;
//Minimum ("Anti Freeze") Temperature.
extern double mqttMinimumFeedTemperature;
//Whether the heating should be off or not
extern bool mqttHeatingSwitch;
//Auxilary Temperature
extern double mqttAuxilaryTemperature;
//Boost will max the feed temperature for mqttBoostTime seconds
extern bool mqttBoost;
//Boost Duration (Seconds)
extern int mqttBoostDuration;
//Countdown variable for boost control.
extern int boostTimeCountdown;
//Fast Heatup. This will max out the feed temperature for a prolongued time until mqttTargetAmbientTemperature has been reached. Setting this to false will return to normal mode in any case.
extern bool mqttFastHeatup;
//Stored Ambient Temperatur as reference. Reset everytime the Fastheatup flag is set
extern double mqttReferenceAmbientTemperature;
//Dynamic Adaption Flag
extern bool mqttDynamicAdaption;
//Take the value from mqttFeedTemperatureSetpoint instead of doing built-in calculations.
extern bool mqttOverrideSetpoint;
//Enable scaling based upon valve opening
extern bool mqttValveScaling;
//Maximum valve opening. 80% for Homematic eTRV-2
extern int mqttMaxValveOpening;
//The value of the valve that is most open
extern int mqttValveOpening;
// Calculated Feed Temperature, Commanded
extern double mqttCommandedFeedTemperature;
// Error Code
extern int mqttErrorCode;

// Auxilary Sensor Feed
extern double mqttAuxFeed;
// Auxilary Sensor Return
extern double mqttAuxReturn;
// Auxilary Sensor Exhaust
extern double mqttAuxExhaust;
// Auxilary Sensor Ambient
extern double mqttAuxAmbient;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void reconnectMqtt();
extern String generateClientId();
extern void setupMqttClient();
extern void callback(char *topic, byte *payload, unsigned int length);
extern void PublishStatus();
extern void PublishHeatingTemperatures();
extern void PublishWaterTemperatures();
extern void PublishAuxilaryTemperatures();

#endif