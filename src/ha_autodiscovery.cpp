#include <ha_autodiscovery.h>
#include <Arduino.h>

const char *HaSensorsFileName = (char *)"/ha_sensors.json";
const char *HaBinarySensorsFileName = (char *)"/ha_binarysensors.json";

/// @brief Create a JSON definition of a HA sensor.
/// @param name Name of the Sensor. Example: "Heating Feed Setpoint Temperature"
/// @param unit_of_measurement °C, KW, ...
/// @param device_class See https://www.home-assistant.io/integrations/sensor/#device-class
/// @param force_update Sends update events even if the value hasn’t changed. Useful if you want to have meaningful value graphs in history. See https://www.home-assistant.io/integrations/sensor.mqtt/#force_update
/// @param sensorShortName A short name for the sensor wich is used inside the discovery topic.
/// @param value_template The template to be used inside HA to correctly parse this value. Example:"{{ value_json.temperature | float(default=0) }}"
void CreateAndPublishAutoDiscoverySensorJson(
    String name,
    String value_template,
    String unit_of_measurement = (char *)"°C",
    String state_topic = "homeassistant/device/state",
    char *device_class = (char *)"temperature",
    bool force_update = true,
    char *sensorShortName = (char *)"temperature")
{
    // This is the discovery topic for this specific sensor
    String discoveryTopic = configuration.HomeAssistant.AutoDiscoveryPrefix + "/sensor/" + configuration.HomeAssistant.DeviceId + "/temperature/config";

    DynamicJsonDocument doc(1024);
    char buffer[256];

    doc["name"] = name;
    doc["uniq_id"] = configuration.HomeAssistant.DeviceId;
    doc["stat_t"] = state_topic;
    doc["unit_of_meas"] = unit_of_measurement;
    doc["dev_cla"] = device_class;
    doc["frc_upd"] = force_update;
    // I'm sending a JSON object as the state of this MQTT device
    // so we'll need to unpack this JSON object to get a single value
    // for this specific sensor.
    doc["val_tpl"] = value_template;

    size_t n = serializeJson(doc, buffer);
    client.publish(configuration.HomeAssistant.StateTopic.c_str(), buffer, n);
}

void SetupAutodiscoveryForAuxSensors()
{
    for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
    {
        String label = configuration.TemperatureSensors.Sensors[i].Label;
        label.replace(" ", "-");
        char *valTempl;
        sprintf(valTempl, "{{ value_json.Auxiliary.%s }}", label.c_str());
        String topic = configuration.HomeAssistant.StateTopic + "Auxiliary/state";
        CreateAndPublishAutoDiscoverySensorJson(
            label.c_str(),
            configuration.HomeAssistant.TempUnit.c_str(),
            valTempl,
            topic);
    }
}

void SetupAutodiscovery(const char *fileName)
{
    // Init SPIFFS
    if (!LittleFS.begin())
    {
        Log.println("SPIFFS Filesystem not ready.");
        return;
    }

    if (!LittleFS.exists(fileName))
    {
        Log.println("HA Autodiscovery file could not be found. Please upload it first.");
        return;
    }

    File file = LittleFS.open(fileName);

    if (!file)
    {
        Log.println("HA Autodiscovery file could not be loaded. Consider checking and reuploading it.");
        return;
    }

    StaticJsonDocument<4096> doc;

    DeserializationError error = deserializeJson(doc, file);

    JsonObject sensors = doc.as<JsonObject>();

    if (error)
    {
        Log.print("deserializeJson() failed: ");
        Log.println(error.c_str());
        return;
    }
    if (configuration.General.Debug)
        Log.println("///----- Reading HA AD Config -----");
    // Sensor Type Category Block: Sensor, Binary Sensor, ...
    for (JsonPair SensorCategory : sensors)
    {
        if (configuration.General.Debug)
            Log.println(SensorCategory.key().c_str());
        // Sensor Device Specific Category like Heating, Water, ...
        JsonObject CurCategoryObj = doc[SensorCategory.key().c_str()].as<JsonObject>();

        for (JsonPair InternalDevCategory : CurCategoryObj)
        {
            if (configuration.General.Debug)
                Log.printf("\t%s\r\n", InternalDevCategory.key().c_str());
            // Specific Internal Device Category Config
            JsonArray CurSensorObject = CurCategoryObj[InternalDevCategory.key().c_str()].as<JsonArray>();

            for (JsonObject SensorConfig : CurSensorObject)
            {
                String curKey;
                for (JsonPair curPair : SensorConfig)
                {
                    curKey = curPair.key().c_str();
                    break;
                }
                if (configuration.General.Debug)
                    Log.printf("\t\t%s\r\n", curKey.c_str());

                JsonObject CurrentSensor = SensorConfig[curKey];
                String discoveryTopic = configuration.HomeAssistant.AutoDiscoveryPrefix + "/" + SensorCategory.key().c_str() + "/" + configuration.HomeAssistant.DeviceId + "/" + curKey + "/config";
                // Replace any whitespaces by dashes
                String label = CurrentSensor["Label"].as<String>();
                label.replace(" ", "-");
                if (label.length() == 0)
                {
                    label = curKey;
                }
                // Topic Abbreviation
                CurrentSensor["~"] = configuration.HomeAssistant.StateTopic + InternalDevCategory.key().c_str();
                CurrentSensor["stat_t"] = "~/state";
                CurrentSensor["name"] = configuration.HomeAssistant.DeviceId + "_" + label;
                CurrentSensor["uniq_id"] = configuration.HomeAssistant.DeviceId + "_" + curKey;                
                CurrentSensor["off_delay"] = configuration.HomeAssistant.OffDelay;
                // Remove "Label" Value because it isn't specified for HA AD
                CurrentSensor.remove("Label");

                // Sensor is assembled. We have to transmit this config to HA now in order to get it working
                char buffer[768];
                size_t n = serializeJson(CurrentSensor, buffer);
                if (configuration.General.Debug)
                {
                    Log.println(discoveryTopic);
                    serializeJsonPretty(CurrentSensor, Serial);
                }
                client.publish(discoveryTopic.c_str(), buffer, n);
            }
        }
    }

    if (configuration.General.Debug)
        Log.println("----- HA AD Config END -----///");

    // Close LittleFS.
    LittleFS.end();
}