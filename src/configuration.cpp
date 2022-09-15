#include <configuration.h>

//——————————————————————————————————————————————————————————————————————————————
//  Configuration File
//——————————————————————————————————————————————————————————————————————————————

const char *configFileName = (char *)"/configuration.json";

//——————————————————————————————————————————————————————————————————————————————
//  MQTT
//——————————————————————————————————————————————————————————————————————————————

char ParametersTopic[255] = "cerasmarter/parameters";

char HeatingTemperaturesTopic[255] = "cerasmarter/heating/parameters";

char WaterTemperaturesTopic[255] = "cerasmarter/water/parameters";

char AuxilaryTemperaturesTopic[255] = "cerasmarter/auxilary/parameters";

char StatusTopic[255] = "cerasmarter/status";

//——————————————————————————————————————————————————————————————————————————————
//  Feature Configuration
//——————————————————————————————————————————————————————————————————————————————

bool AuxSensorsEnabled = false;

bool HeatingTemperaturesEnabled = true;

bool WaterTemperaturesEnabled = false;

Configuration configuration;

bool ReadConfiguration()
{
    // Init SPIFFS
    if (!SPIFFS.begin())
        SPIFFS.begin(true);

    if (!SPIFFS.exists(configFileName))
    {
        WriteToConsoles("Configuration file could not be found. Please upload it first.");
        return false;
    }

    File file = SPIFFS.open(configFileName);

    if (!file)
    {
        WriteToConsoles("Configuration file could not be loaded. Consider checking and reuploading it.");
        return false;
    }

    // Open and parse the file
    const int docSize = 1024;
    StaticJsonDocument<docSize> doc;
    DeserializationError error = deserializeJson(doc, file);

    if (error)
    {
        WriteToConsoles("Error processing configuration: ");
        WriteToConsoles(error.c_str());
        WriteToConsoles("\r\n");
        return false;
    }

    serializeJsonPretty(doc, Serial);
    strlcpy(configuration.Wifi_SSID, doc["Wifi"]["SSID"], sizeof(configuration.Wifi_SSID));             // "ssid"
    strlcpy(configuration.Wifi_Password, doc["Wifi"]["Password"], sizeof(configuration.Wifi_Password)); // "pass"
    strlcpy(configuration.Wifi_Hostname, doc["Wifi"]["Hostname"], sizeof(configuration.Wifi_Hostname)); // "CERASMARTER"

    JsonObject MQTT = doc["MQTT"];
    strlcpy(configuration.MQTT_Server, MQTT["Server"], sizeof(configuration.MQTT_Server));       // "192.168.1.123"
    configuration.MQTT_Port = MQTT["Port"];                                                      // 1883
    strlcpy(configuration.MQTT_User, MQTT["User"], sizeof(configuration.MQTT_User));             // "user"
    strlcpy(configuration.MQTT_Password, MQTT["Password"], sizeof(configuration.MQTT_Password)); // "pass"

    JsonObject MQTT_Topics = MQTT["Topics"];

    strlcpy(configuration.MQTT_Topics_HeatingParameters, MQTT_Topics["HeatingParameters"], sizeof(configuration.MQTT_Topics_HeatingParameters));    //"cerasmarter/heating/parameters"
    strlcpy(configuration.MQTT_Topics_WaterParameters, MQTT_Topics["WaterParameters"], sizeof(configuration.MQTT_Topics_WaterParameters));          //"cerasmarter/water/parameters"
    strlcpy(configuration.MQTT_Topics_AuxilaryParameters, MQTT_Topics["AuxilaryParameters"], sizeof(configuration.MQTT_Topics_AuxilaryParameters)); //"cerasmarter/auxilary/parameters"
    strlcpy(configuration.MQTT_Topics_Status, MQTT_Topics["Status"], sizeof(configuration.MQTT_Topics_Status));                                     // "cerasmarter/status"

    JsonObject Features = doc["Features"];
    configuration.Features_HeatingParameters = Features["HeatingParameters"];   // true
    configuration.Features_WaterParameters = Features["WaterParameters"];       // false
    configuration.Features_AuxilaryParameters = Features["AuxilaryParameters"]; // false

    JsonObject TimeSettings = doc["Time"];
    strlcpy(configuration.Time_Timezone, TimeSettings["Timezone"], sizeof(configuration.Time_Timezone)); // true

    JsonObject GeneralSettings = doc["General"];
    configuration.BusMessageTimeout = GeneralSettings["BusMessageTimeout"];
    configuration.Debug = GeneralSettings["Debug"];

    JsonObject Leds = doc["LEDs"];
    configuration.WifiLed = Leds["Wifi"];       // 26
    configuration.StatusLed = Leds["Status"];   // 27
    configuration.MqttLed = Leds["Mqtt"];       // 14
    configuration.HeatingLed = Leds["Heating"]; // 25

    JsonObject CAN = doc["CAN"];
    configuration.CAN_SCK = CAN["SCK"];
    configuration.CAN_MISO = CAN["MISO"];
    configuration.CAN_MOSI = CAN["MOSI"];
    configuration.CAN_CS = CAN["CS"];
    configuration.CAN_INT = CAN["INT"];
    configuration.CAN_Quartz = CAN["Quartz"];


    // We don't need to keep it open at this point.
    SPIFFS.end();

    return true;
}
