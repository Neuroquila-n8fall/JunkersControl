#ifndef _MQTT_CONFIG_H
#define _MQTT_CONFIG_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <arduino_secrets.h>
#include <wifi_config.h>

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Client (uses Wifi Client)
//——————————————————————————————————————————————————————————————————————————————
PubSubClient client(espClient);

//-- MQTT Endpoint
const char *mqttServer = mqttSERVER;
const char *mqttUsername = mqttUSERNAME;
const char *mqttPassword = mqttPASSWORD;

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

//Heating Adaption: The Outside temperature at which hcMaxFeed should be reached
const char subscription_FeedBaseSetpoint[] = "heizung/parameters/fusspunkt";

//Heating Adaption: The Outside temperature at which the minimum temperature should be set
const char subscription_FeedCutOff[] = "heizung/parameters/endpunkt";

//Heating Adaption: Minimum Temperature of Feed (Anti-Freeze)
const char subscription_FeedMinimum[] = "heizung/parameters/minimum";

//Secondary Outside Temperature Source
const char subscription_AuxTemperature[] = "umwelt/temperaturen/aussen";

//Ambient Temperature Source
const char subscription_AmbientTemperature[] = "heizung/parameters/ambient";

//Ambient Temperature Target
const char subscription_TargetAmbientTemperature[] = "heizung/parameters/targetambient";

//On-Demand Boost.
const char subscription_OnDemandBoost[] = "heizung/control/boost";

//On-Demand Boost: Time in seconds
const char subscription_OnDemandBoostDuration[] = "heizung/parameters/boostduration";

//Fast-Heatup
const char subscription_FastHeatup[] = "heizung/control/fastheatup";

//Adaption
const char subscription_Adaption[] = "heizung/parameters/adaption";

//Dynamic Adaption based on return temperature sensor and desired target room temperature
//  This enables a mode in which the feed temperature will be decreased when the return temperature is much higher than required
//  This is useful when the heating is much more capable than required. 
//  Example: The return Temperature is 40°, the desired room temperature is 21°: The Adaption is -19° --> Feed setpoint is decreased by 19°
//  Another one: The Return temperature is 20°, the desired room temperature is 21°: The Adaption is +1° --> Feed setpoint increased by 1°
const char subscription_DynamicAdaption[] = "heizung/parameters/dynadapt";



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

//Boost
const char pub_Boost[] = "heizung/status/boost";

//Fast Heatup
const char pub_Fastheatup[] = "heizung/status/schnellaufheizung";



//-- Topics for Temperature Sensor Readings

//Feed Temperature Topic
const char pub_SensorFeed[] = "heizung/temperaturen/vorlauf";

//Return Temperature Topic
const char pub_SensorReturn[] = "heizung/temperaturen/nachlauf";

//Exhaust Temperature Topic
const char pub_SensorExhaust[] = "heizung/temperaturen/abgas";

//Ambient Temperature Topic
const char pub_SensorAmbient[] = "heizung/temperaturen/umgebung";

#endif