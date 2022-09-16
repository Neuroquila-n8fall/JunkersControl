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
};

struct Configuration
{
    struct Wifi_
    {
        char SSID[255];     // "ssid"
        char Password[255]; // "pass"
        char Hostname[255]; // "CERASMARTER"
    } Wifi;

    struct Mqtt_
    {
        char Server[15];    // "192.168.1.123"
        int Port;           // "Default: 1883"
        char User[255];     // "user"
        char Password[255]; // "pass"

        struct Topics_
        {
            char HeatingParameters[255];
            char WaterParameters[255];
            char AuxilaryParameters[255];
            char Status[255]; // "cerasmarter/status"
        } Topics;

    } Mqtt;

    struct Features_
    {
        bool Features_HeatingParameters;  // true
        bool Features_WaterParameters;    // false
        bool Features_AuxilaryParameters; // false
    } Features;

    struct General_
    {
        char Time_Timezone[255]; // Timezone to be used for NTP, i.e. Europe/Berlin

        int BusMessageTimeout; // Message Timeout from other controllers on the bus, ex. 30
        bool Debug;            // Output debug messages, true|false
        bool Sniffing;         // Output every CAN message von the bus
    } General;

    struct LEDs_
    {
        int StatusLed;  // Status LED GPIO, ex. 27
        int WifiLed;    // Wifi Status LED GPIO, ex. 26
        int MqttLed;    // Mqtt Led GPIO, ex. 14
        int HeatingLed; // Heating Active LED GPIO, ex. 25
    } LEDs;

    struct CanModuleConfig_
    {
        byte CAN_SCK;
        byte CAN_MISO;
        byte CAN_MOSI;
        byte CAN_CS;
        byte CAN_INT;
        int CAN_Quartz;
    } CanModuleConfig;

    struct CanAddresses_
    {
        struct Heating_
        {
            byte FeedCurrent;
            byte FeedMax;
            byte FeedSetpoint;
            byte OutsideTemperature;
            byte Pump;
            byte Season;
            byte Operation;
            byte Power;
        } Heating;

        struct General_
        {
            byte FlameLit;
            byte DateTime;
            byte Error;
        } General;

        struct HotWater_
        {
            byte SetpointTemperature;
            byte MaxTemperature;
            byte CurrentTemperature;
            byte Now;
            byte BufferOperation;
            byte ContinousFlowSetpointTemperature;
        } HotWater;

        struct MixedCircuit_
        {
            byte Pump;
            byte FeedSetpoint;
            byte FeedCurrent;
            byte Economy;
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
