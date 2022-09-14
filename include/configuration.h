#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <telnet.h>

//——————————————————————————————————————————————————————————————————————————————
//  Funcs
//——————————————————————————————————————————————————————————————————————————————

extern bool ReadConfiguration();

//——————————————————————————————————————————————————————————————————————————————
//  Configuration File
//——————————————————————————————————————————————————————————————————————————————

struct Configuration
{
    char Wifi_SSID[255];     // "ssid"
    char Wifi_Password[255]; // "pass"
    char Wifi_Hostname[255]; // "CERASMARTER"

    char MQTT_Server[15];   // "192.168.1.123"
    int MQTT_Port;             // "1883"
    char MQTT_User[255];     // "user"
    char MQTT_Password[255]; // "pass"

    char MQTT_Topics_HeatingParameters[255];
    char MQTT_Topics_WaterParameters[255];
    char MQTT_Topics_AuxilaryParameters[255];
    char MQTT_Topics_Status[255]; // "cerasmarter/status"

    bool Features_HeatingParameters;  // true
    bool Features_WaterParameters;    // false
    bool Features_AuxilaryParameters; // false

    char Time_Timezone[255]; // Timezone to be used for NTP, i.e. Europe/Berlin

    int BusMessageTimeout; // Message Timeout from other controllers on the bus, ex. 30
    bool Debug;            // Output debug messages, true|false

    int StatusLed;  // Status LED GPIO, ex. 27
    int WifiLed;    // Wifi Status LED GPIO, ex. 26
    int MqttLed;    // Mqtt Led GPIO, ex. 14
    int HeatingLed; // Heating Active LED GPIO, ex. 25
};

extern const char *configFileName;
extern Configuration configuration;

//——————————————————————————————————————————————————————————————————————————————
//  MQTT
//——————————————————————————————————————————————————————————————————————————————

// Topic where Parameters (settings) are received
extern char ParametersTopic[];

// Topic where heating temperatures are published at
extern char HeatingTemperaturesTopic[255];
// Topic where water temperatures are published at
extern char WaterTemperaturesTopic[255];
// Topic where temperatures of external temperature sensors are published at
extern char AuxilaryTemperaturesTopic[255];
// Topic where the status should be published at
extern char StatusTopic[255];

//——————————————————————————————————————————————————————————————————————————————
//  Feature Configuration
//——————————————————————————————————————————————————————————————————————————————

// Whether external temperature sensors are enabled
extern bool AuxSensorsEnabled;
// Whether heating temperatures should be published
extern bool HeatingTemperaturesEnabled;
// Whether water temperatures should be published
extern bool WaterTemperaturesEnabled;

#endif // CONFIGURATION_H
