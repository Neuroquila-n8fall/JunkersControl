#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <telnet.h>
#include <main.h>
#include <DallasTemperature.h>

//——————————————————————————————————————————————————————————————————————————————
//  Funcs
//——————————————————————————————————————————————————————————————————————————————

extern bool ReadConfiguration();

//——————————————————————————————————————————————————————————————————————————————
//  Configuration File
//——————————————————————————————————————————————————————————————————————————————

struct Sensor
{
    char Label[255];
    bool UseAsReturnValueReference;
    DeviceAddress Address;
    bool reachable;
};

struct Configuration
{
    struct Wifi_
    {
        char SSID[255];     // "ssid"
        char Password[255]; // "pass"
        char Hostname[255] = "CERASMARTER"; // "CERASMARTER"
    } Wifi;

    struct Mqtt_
    {
        char Server[15];    // "192.168.1.123"
        int Port = 1883;           // "Default: 1883"
        char User[255];     // "user"
        char Password[255]; // "pass"

        struct Topics_
        {
            char HeatingValues[255];
            char HeatingParameters[255];
            char WaterValues[255];
            char WaterParameters[255];
            char AuxilaryValues[255];
            char Status[255]; 
            char StatusRequest[255];
            char Boost[255];
            char FastHeatup[255];
        } Topics;

    } Mqtt;

    struct Features_
    {
        bool HeatingParameters;  // true
        bool WaterParameters;    // false
        bool AuxilaryParameters; // false
    } Features;

    struct General_
    {
        char Timezone[255]; // Timezone to be used for NTP, i.e. Europe/Berlin

        int BusMessageTimeout; // Message Timeout from other controllers on the bus, ex. 30
        bool Debug;            // Output debug messages, true|false
        bool Sniffing;         // Output every CAN message von the bus
    } General;

    struct HomeAssistant_
    {
        String AutoDiscoveryPrefix;
        bool Enabled = false;
        int OffDelay;
        String DeviceId;
        String StateTopic;
        String TempUnit;
    } HomeAssistant;

    struct LEDs_
    {
        int StatusLed;  // Status LED GPIO, ex. 27
        int WifiLed;    // Wifi Status LED GPIO, ex. 26
        int MqttLed;    // Mqtt Led GPIO, ex. 14
        int HeatingLed; // Heating Active LED GPIO, ex. 25
    } LEDs;

    struct CanModuleConfig_
    {
        int CAN_Quartz;
    } CanModuleConfig;

    struct CanAddresses_
    {
        struct Heating_
        {
            uint16_t FeedCurrent;
            uint16_t FeedMax;
            uint16_t FeedSetpoint;
            uint16_t OutsideTemperature;
            uint16_t Pump;
            uint16_t Season;
            uint16_t Operation;
            uint16_t Power;
            uint16_t Mode;
            uint16_t Economy;
        } Heating;

        struct General_
        {
            uint16_t FlameLit;
            uint16_t DateTime;
            uint16_t Error;
        } General;

        struct HotWater_
        {
            uint16_t SetpointTemperature;
            uint16_t MaxTemperature;
            uint16_t CurrentTemperature;
            uint16_t Now;
            uint16_t BufferOperation;
            uint16_t ContinousFlowSetpointTemperature;
        } HotWater;

        struct MixedCircuit_
        {
            uint16_t Pump;
            uint16_t FeedSetpoint;
            uint16_t FeedCurrent;
            uint16_t Economy;
        } MixedCircuit;

    } CanAddresses;

    struct TemperatureSensors_
    {
        int SensorCount;
        Sensor *Sensors;
    } TemperatureSensors;
};

extern const char *configFileName;
extern Configuration configuration;

#endif // CONFIGURATION_H
