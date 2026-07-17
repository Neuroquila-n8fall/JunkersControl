#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <telnet.h>
#include <main.h>
#include <DallasTemperature.h>

//——————————————————————————————————————————————————————————————————————————————
//  Funcs
//——————————————————————————————————————————————————————————————————————————————

extern bool ReadConfiguration();

extern bool WriteConfiguration();

extern void SetConfigurationUploadPending(bool pending);

extern String IntToHex(int value);

extern unsigned long convertHexString(const char *src);

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
        char Server[16];    // "192.168.123.123"
        int Port = 1883;           // "Default: 1883"
        char User[255];     // "user"
        char Password[255]; // "pass"

        struct Topics_
        {
            char HeatingValues[255];
            char HeatingParameters[255];
            char WaterValues[255];
            char WaterParameters[255];
            char AuxiliaryValues[255];
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
        bool AuxiliaryParameters; // false
        bool UseAuxiliaryOutsideTempReference; //True: Use the value from "commandedValues.Heating.AuxiliaryTemperature" instead of the reading from the heating sensor.
    } Features;

    struct General_
    {
        char Timezone[255]; // Timezone to be used for NTP, i.e. Europe/Berlin
        char PosixTimezone[128] = "CET-1CEST,M3.5.0,M10.5.0/3"; // Offline-capable local time rules

        int BusMessageTimeout; // Message Timeout from other controllers on the bus, ex. 30
        bool Debug;            // Output debug messages, true|false
        bool Sniffing;         // Output every CAN message von the bus
    } General;

    struct FailSafe_
    {
        bool Enabled = true;
        unsigned long CommandTimeoutSeconds = 300;
        int StartHour = 5;
        int StartMinute = 30;
        int StopHour = 23;
        int StopMinute = 30;
        bool HeatWhenTimeUnknown = true;
        double BasepointTemperature = -10.0F;
        double EndpointTemperature = 31.0F;
        double MinimumFeedTemperature = 10.0F;
        double MaximumFeedTemperature = 55.0F;
    } FailSafe;

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
        int StatusLed = 27;  // Status LED GPIO, ex. 27
        int WifiLed = 26;    // Wifi Status LED GPIO, ex. 26
        int MqttLed = 14;    // Mqtt Led GPIO, ex. 14
        int HeatingLed = 25; // Heating Active LED GPIO, ex. 25
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
