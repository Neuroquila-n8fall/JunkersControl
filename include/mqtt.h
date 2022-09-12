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