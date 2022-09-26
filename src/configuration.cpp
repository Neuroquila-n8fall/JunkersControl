#include <configuration.h>

//——————————————————————————————————————————————————————————————————————————————
//  Configuration File
//——————————————————————————————————————————————————————————————————————————————

const char *configFileName = (char *)"/configuration.json";

Configuration configuration;

// Converters

void convertFromJson(JsonVariantConst src, byte& dst)
{    
    dst = strtoul(src.as<const char *>(), NULL, 16);
}

unsigned long convertHexString(const char *src)
{
    byte addr = strtoul(src, NULL, 16);
    return strtoul(src, NULL, 16);
}

bool ReadConfiguration()
{
    // Init SPIFFS
    if (!SPIFFS.begin())
        SPIFFS.begin(true);

    if (!SPIFFS.exists(configFileName))
    {
        Log.println("Configuration file could not be found. Please upload it first.");
        return false;
    }

    File file = SPIFFS.open(configFileName);

    if (!file)
    {
        Log.println("Configuration file could not be loaded. Consider checking and reuploading it.");
        return false;
    }

    // Open and parse the file
    const int docSize = 4096;
    StaticJsonDocument<docSize> doc;
    DeserializationError error = deserializeJson(doc, file);

    if (error)
    {
        Log.printf("Error processing configuration: %s\r\n", error.c_str());
        return false;
    }

    if (Debug)
    {
        serializeJsonPretty(doc, Log);
    }

    strlcpy(configuration.Wifi.SSID, doc["Wifi"]["SSID"], sizeof(configuration.Wifi.SSID));             // "ssid"
    strlcpy(configuration.Wifi.Password, doc["Wifi"]["Password"], sizeof(configuration.Wifi.Password)); // "pass"
    strlcpy(configuration.Wifi.Hostname, doc["Wifi"]["Hostname"], sizeof(configuration.Wifi.Hostname)); // "CERASMARTER"

    JsonObject MQTT = doc["MQTT"];
    strlcpy(configuration.Mqtt.Server, MQTT["Server"], sizeof(configuration.Mqtt.Server));       // "192.168.1.123"
    configuration.Mqtt.Port = MQTT["Port"];                                                      // 1883
    strlcpy(configuration.Mqtt.User, MQTT["User"], sizeof(configuration.Mqtt.User));             // "user"
    strlcpy(configuration.Mqtt.Password, MQTT["Password"], sizeof(configuration.Mqtt.Password)); // "pass"

    JsonObject MQTT_Topics = MQTT["Topics"];
    strlcpy(configuration.Mqtt.Topics.HeatingValues, MQTT_Topics["HeatingValues"], sizeof(configuration.Mqtt.Topics.HeatingValues));    //"cerasmarter/heating/values"
    strlcpy(configuration.Mqtt.Topics.WaterValues, MQTT_Topics["WaterValues"], sizeof(configuration.Mqtt.Topics.WaterValues));          //"cerasmarter/water/values"
    strlcpy(configuration.Mqtt.Topics.HeatingParameters, MQTT_Topics["HeatingParameters"], sizeof(configuration.Mqtt.Topics.HeatingParameters));    //"cerasmarter/heating/parameters"
    strlcpy(configuration.Mqtt.Topics.WaterParameters, MQTT_Topics["WaterParameters"], sizeof(configuration.Mqtt.Topics.WaterParameters));          //"cerasmarter/water/parameters"
    strlcpy(configuration.Mqtt.Topics.AuxilaryValues, MQTT_Topics["AuxilaryParameters"], sizeof(configuration.Mqtt.Topics.AuxilaryValues)); //"cerasmarter/auxilary/parameters"
    strlcpy(configuration.Mqtt.Topics.Status, MQTT_Topics["Status"], sizeof(configuration.Mqtt.Topics.Status));                                     // "cerasmarter/status"
    strlcpy(configuration.Mqtt.Topics.StatusRequest, MQTT_Topics["StatusRequest"], sizeof(configuration.Mqtt.Topics.StatusRequest));                                     // "cerasmarter/status/get"

    JsonObject Features = doc["Features"];
    configuration.Features.Features_HeatingParameters = Features["HeatingParameters"];   // true
    configuration.Features.Features_WaterParameters = Features["WaterParameters"];       // false
    configuration.Features.Features_AuxilaryParameters = Features["AuxilaryParameters"]; // false

    JsonObject TimeSettings = doc["Time"];
    strlcpy(configuration.General.Time_Timezone, TimeSettings["Timezone"], sizeof(configuration.General.Time_Timezone)); // true

    JsonObject GeneralSettings = doc["General"];
    configuration.General.BusMessageTimeout = GeneralSettings["BusMessageTimeout"];
    configuration.General.Debug = GeneralSettings["Debug"];
    configuration.General.Sniffing = GeneralSettings["Sniffing"];

    JsonObject HomeAssistantSettings = doc["HomeAssistant"];
    configuration.HomeAssistant.Enabled = HomeAssistantSettings["Enabled"];
    configuration.HomeAssistant.DeviceId = HomeAssistantSettings["DeviceId"].as<String>();
    configuration.HomeAssistant.OffDelay = HomeAssistantSettings["OffDelay"];
    configuration.HomeAssistant.AutoDiscoveryPrefix = HomeAssistantSettings["AutoDiscoveryPrefix"].as<String>();
    configuration.HomeAssistant.StateTopic = configuration.HomeAssistant.AutoDiscoveryPrefix + "/" + configuration.HomeAssistant.DeviceId + "/";
    configuration.HomeAssistant.TempUnit = HomeAssistantSettings["TempUnit"].as<String>();
    

    JsonObject Leds = doc["LEDs"];
    configuration.LEDs.WifiLed = Leds["Wifi"];       // 26
    configuration.LEDs.StatusLed = Leds["Status"];   // 27
    configuration.LEDs.MqttLed = Leds["Mqtt"];       // 14
    configuration.LEDs.HeatingLed = Leds["Heating"]; // 25

    JsonObject CAN = doc["CAN"];    
    configuration.CanModuleConfig.CAN_Quartz = CAN["Quartz"];

    JsonObject CAN_Addresses = CAN["Addresses"];

    JsonObject CAN_Addresses_Controller = CAN_Addresses["Controller"];
    configuration.CanAddresses.General.FlameLit = convertHexString(CAN_Addresses_Controller["FlameStatus"].as<const char*>()); // "0x209"
    configuration.CanAddresses.General.Error = convertHexString(CAN_Addresses_Controller["Error"].as<const char*>());         // "0x206"
    configuration.CanAddresses.General.DateTime = convertHexString(CAN_Addresses_Controller["DateTime"].as<const char*>());    // "0x256"

    JsonObject CAN_Addresses_Heating = CAN_Addresses["Heating"];
    configuration.CanAddresses.Heating.FeedCurrent = convertHexString(CAN_Addresses_Heating["FeedCurrent"].as<const char*>());               // "0x201"
    configuration.CanAddresses.Heating.FeedMax = convertHexString(CAN_Addresses_Heating["FeedMax"].as<const char*>());                       // "0x200"
    configuration.CanAddresses.Heating.FeedSetpoint = convertHexString(CAN_Addresses_Heating["FeedSetpoint"].as<const char*>());             // "0x252"
    configuration.CanAddresses.Heating.OutsideTemperature = convertHexString(CAN_Addresses_Heating["OutsideTemperature"].as<const char*>()); // "0x207"
    configuration.CanAddresses.Heating.Pump = convertHexString(CAN_Addresses_Heating["Pump"].as<const char*>());                             // "0x20A"
    configuration.CanAddresses.Heating.Season = convertHexString(CAN_Addresses_Heating["Season"].as<const char*>());                         // "0x20C"
    configuration.CanAddresses.Heating.Operation = convertHexString(CAN_Addresses_Heating["Operation"].as<const char*>());                   // "0x250"
    configuration.CanAddresses.Heating.Power = convertHexString(CAN_Addresses_Heating["Power"].as<const char*>());                           // "0x251"
    configuration.CanAddresses.Heating.Mode = convertHexString(CAN_Addresses_Heating["Mode"].as<const char*>());                             // "0x258"
    configuration.CanAddresses.Heating.Economy = convertHexString(CAN_Addresses_Heating["Economy"].as<const char*>());                       // "0x253"

    JsonObject CAN_Addresses_HotWater = CAN_Addresses["HotWater"];
    configuration.CanAddresses.HotWater.SetpointTemperature = convertHexString(CAN_Addresses_HotWater["SetpointTemperature"].as<const char*>()); // "0x203"
    configuration.CanAddresses.HotWater.MaxTemperature = convertHexString(CAN_Addresses_HotWater["MaxTemperature"].as<const char*>());           // "0x204"
    configuration.CanAddresses.HotWater.CurrentTemperature = convertHexString(CAN_Addresses_HotWater["CurrentTemperature"].as<const char*>());   // "0x205"
    configuration.CanAddresses.HotWater.Now = convertHexString(CAN_Addresses_HotWater["Now"].as<const char*>());                                 // "0x254"
    configuration.CanAddresses.HotWater.BufferOperation = convertHexString(CAN_Addresses_HotWater["BufferOperation"].as<const char*>());         // "0x20B"

    configuration
        .CanAddresses
        .HotWater
        .ContinousFlowSetpointTemperature = convertHexString(CAN_Addresses_HotWater["ContinousFlow"]["SetpointTemperature"].as<const char*>()); // "0x255"

    JsonObject CAN_Addresses_MixedCircuit = CAN_Addresses["MixedCircuit"];
    configuration.CanAddresses.MixedCircuit.Pump = convertHexString(CAN_Addresses_MixedCircuit["Pump"].as<const char*>());                 // "0x404"
    configuration.CanAddresses.MixedCircuit.FeedSetpoint = convertHexString(CAN_Addresses_MixedCircuit["FeedSetpoint"].as<const char*>()); // "0x405"
    configuration.CanAddresses.MixedCircuit.FeedCurrent = convertHexString(CAN_Addresses_MixedCircuit["FeedCurrent"].as<const char*>());   // "0x440"
    configuration.CanAddresses.MixedCircuit.Economy = convertHexString(CAN_Addresses_MixedCircuit["Economy"].as<const char*>());           // "0x407"


    int curSensor = 0;
    bool tempReferenceSensorSet = false;
    
    JsonArray sensors = doc["AuxilarySensors"]["Sensors"].as<JsonArray>();

    const int sensorCount = sensors.size();

    // Resize the Temperature array
    ceraValues.Auxilary.Temperatures = (double *)malloc(sensorCount * sizeof(double));

    // Set initial values to zero.
    for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
    {
        ceraValues.Auxilary.Temperatures[i] = 0.0F;
    }

    // Init Sensors: This might cause trouble if too many sensors are added... we'll have to see where this is going.
    configuration.TemperatureSensors.Sensors = (Sensor *)malloc(sensorCount * sizeof(Sensor));

    // Set the amount of sensors
    configuration.TemperatureSensors.SensorCount = sensorCount;

    for (JsonObject AuxilarySensors_Sensor : sensors)
    {
        Sensor newSensor;
        strlcpy(newSensor.Label, AuxilarySensors_Sensor["Label"], sizeof(newSensor.Label)); // true
        newSensor.UseAsReturnValueReference = AuxilarySensors_Sensor["IsReturnValue"].as<bool>();
        JsonArray AuxilarySensors_Sensor_Address = AuxilarySensors_Sensor["Address"];
        int i = 0;
        for (JsonVariant value : AuxilarySensors_Sensor_Address)
        {
            byte addrByte = strtoul(value.as<const char *>(), NULL, 16);
            newSensor.Address[i++] = addrByte;
        }
        configuration.TemperatureSensors.Sensors[curSensor++] = newSensor;
        if(Debug)
        {
            if (newSensor.UseAsReturnValueReference && tempReferenceSensorSet)
            {
                Log.printf("WARN: Sensor #%i is set as temperature reference but another sensor has been already set.");
            }            
            if(newSensor.UseAsReturnValueReference)
            {
                Log.println("INFO: The following sensor will be used as a return temperature reference.");
                tempReferenceSensorSet = true;
            }
            Log.printf("Added Sensor #%i with Label '%s'\r\n", curSensor, newSensor.Label);
        }
    }


    // We don't need to keep it open at this point.
    SPIFFS.end();

    return true;
}
