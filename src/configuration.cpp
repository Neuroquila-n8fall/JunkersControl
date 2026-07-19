#include <configuration.h>
#include <Preferences.h>

//——————————————————————————————————————————————————————————————————————————————
//  Configuration File
//——————————————————————————————————————————————————————————————————————————————

const char *configFileName = (char *)"/configuration.json";
const char *temporaryConfigFileName = (char *)"/configuration.tmp";
const char *backupConfigFileName = (char *)"/configuration.bak";

Configuration configuration;

static bool configurationUploadPending = false;
static const char *filesystemBackupNamespace = "cerafsbackup";
static const char *filesystemBackupKey = "config";
static const size_t maximumBackupSize = 16384;

static bool validateConfigurationDocument(JsonDocument &doc, String &errorMessage)
{
    const char *requiredSections[] = {"Wifi", "MQTT", "Features", "Time", "General", "HomeAssistant", "CAN", "AuxiliarySensors", "LEDs"};
    for (const char *section : requiredSections)
    {
        if (!doc[section].is<JsonObject>())
        {
            errorMessage = "Configuration section is missing or invalid: " + String(section);
            return false;
        }
    }
    return true;
}

static bool isProvisioningTemplate(JsonDocument &doc)
{
    const String ssid = doc["Wifi"]["SSID"] | "";
    JsonVariant marker = doc["ProvisioningTemplate"];
    if (!marker.isNull())
    {
        // A custom filesystem build is explicitly stamped false. A true marker
        // still requires empty Wi-Fi credentials so a customized/uploaded copy
        // of the template is not mistaken for a release provisioning image.
        return marker.as<bool>() && ssid.isEmpty();
    }

    // Filesystem images produced before the explicit marker was introduced
    // used this credential-free combination.
    const String mqttServer = doc["MQTT"]["Server"] | "";
    const String mqttUser = doc["MQTT"]["User"] | "";
    return ssid.isEmpty() && mqttServer == "1.2.3.4" && mqttUser == "mqtt";
}

bool BackupConfigurationForFilesystemUpdate(String &errorMessage)
{
    File file = LittleFS.open(configFileName, FILE_READ);
    if (!file)
    {
        errorMessage = "The active configuration could not be opened; filesystem update cancelled.";
        return false;
    }
    const size_t size = file.size();
    if (size == 0 || size > maximumBackupSize)
    {
        file.close();
        errorMessage = "The active configuration has an unsupported size; filesystem update cancelled.";
        return false;
    }
    std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[size]);
    if (!data || file.read(data.get(), size) != size)
    {
        file.close();
        errorMessage = "The active configuration could not be read; filesystem update cancelled.";
        return false;
    }
    file.close();
    JsonDocument doc;
    DeserializationError jsonError = deserializeJson(doc, data.get(), size);
    if (jsonError || !validateConfigurationDocument(doc, errorMessage))
    {
        if (jsonError)
            errorMessage = "The active configuration is invalid JSON; filesystem update cancelled.";
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(filesystemBackupNamespace, false))
    {
        errorMessage = "NVS backup storage could not be opened; filesystem update cancelled.";
        return false;
    }
    const size_t existingSize = preferences.getBytesLength(filesystemBackupKey);
    if (existingSize == size)
    {
        std::unique_ptr<uint8_t[]> existingData(new (std::nothrow) uint8_t[size]);
        if (existingData && preferences.getBytes(filesystemBackupKey, existingData.get(), size) == size &&
            memcmp(existingData.get(), data.get(), size) == 0)
        {
            preferences.end();
            return true;
        }
    }
    const size_t written = preferences.putBytes(filesystemBackupKey, data.get(), size);
    preferences.end();
    if (written != size)
    {
        errorMessage = "The configuration could not be backed up to NVS; filesystem update cancelled.";
        return false;
    }
    return true;
}

bool PersistConfigurationBackup(String &errorMessage)
{
    return BackupConfigurationForFilesystemUpdate(errorMessage);
}

bool ClearPersistentConfigurationBackup(String &errorMessage)
{
    Preferences preferences;
    if (!preferences.begin(filesystemBackupNamespace, false))
    {
        errorMessage = "NVS backup storage could not be opened.";
        return false;
    }
    const bool cleared = preferences.clear();
    preferences.end();
    if (!cleared)
    {
        errorMessage = "The persistent configuration backup could not be cleared.";
        return false;
    }
    return true;
}

bool RestoreConfigurationAfterFilesystemUpdate(String &errorMessage)
{
    Preferences preferences;
    if (!preferences.begin(filesystemBackupNamespace, false))
        return true;
    const size_t size = preferences.getBytesLength(filesystemBackupKey);
    if (size == 0)
    {
        preferences.end();
        return true;
    }
    if (size > maximumBackupSize)
    {
        preferences.remove(filesystemBackupKey);
        preferences.end();
        errorMessage = "Discarded an oversized configuration backup from NVS.";
        return false;
    }
    std::unique_ptr<uint8_t[]> data(new (std::nothrow) uint8_t[size]);
    if (!data || preferences.getBytes(filesystemBackupKey, data.get(), size) != size)
    {
        preferences.end();
        errorMessage = "The configuration backup could not be read from NVS.";
        return false;
    }
    JsonDocument doc;
    DeserializationError jsonError = deserializeJson(doc, data.get(), size);
    if (jsonError || !validateConfigurationDocument(doc, errorMessage))
    {
        preferences.remove(filesystemBackupKey);
        preferences.end();
        if (jsonError)
            errorMessage = "Discarded an invalid configuration backup from NVS.";
        return false;
    }

    // A deliberately uploaded, valid device configuration is authoritative.
    // Restore only over a release provisioning template, a missing file, or an
    // invalid file left by an interrupted/raw filesystem replacement.
    File currentFile = LittleFS.open(configFileName, FILE_READ);
    if (currentFile)
    {
        JsonDocument currentDoc;
        String currentValidationError;
        const DeserializationError currentJsonError = deserializeJson(currentDoc, currentFile);
        currentFile.close();
        if (!currentJsonError && validateConfigurationDocument(currentDoc, currentValidationError) &&
            !isProvisioningTemplate(currentDoc))
        {
            preferences.end();
            return true;
        }
    }
    const char *restoreFileName = "/configuration.restore";
    File restored = LittleFS.open(restoreFileName, FILE_WRITE, true);
    if (!restored || restored.write(data.get(), size) != size)
    {
        restored.close();
        LittleFS.remove(restoreFileName);
        preferences.end();
        errorMessage = "The preserved configuration could not be restored to LittleFS.";
        return false;
    }
    restored.flush();
    restored.close();
    LittleFS.remove(backupConfigFileName);
    if (LittleFS.exists(configFileName) && !LittleFS.rename(configFileName, backupConfigFileName))
    {
        LittleFS.remove(restoreFileName);
        preferences.end();
        errorMessage = "The filesystem template could not be replaced by the preserved configuration.";
        return false;
    }
    if (!LittleFS.rename(restoreFileName, configFileName))
    {
        LittleFS.rename(backupConfigFileName, configFileName);
        preferences.end();
        errorMessage = "The preserved configuration could not be committed.";
        return false;
    }
    LittleFS.remove(backupConfigFileName);
    preferences.end();
    Log.println("Restored the persistent configuration backup after the filesystem was replaced.");
    return true;
}

void SetConfigurationUploadPending(bool pending)
{
    configurationUploadPending = pending;
}

// Converters

unsigned long convertHexString(const char *src)
{
    return strtoul(src, NULL, 16);
}

String IntToHex(int value)
{
    char buf[6];
    sprintf(buf, "0x%.3X", value);
    return buf;
}

bool ReadConfiguration()
{
    if (!LittleFS.exists(configFileName) && LittleFS.exists(backupConfigFileName))
    {
        Log.println("Recovering configuration from backup.");
        LittleFS.rename(backupConfigFileName, configFileName);
    }

    if (!LittleFS.exists(configFileName))
    {
        Log.println("Configuration file could not be found. Please upload it first.");
        return false;
    }

    File file = LittleFS.open(configFileName);

    if (!file)
    {
        Log.println("Configuration file could not be loaded. Consider checking and reuploading it.");
        file.close();
        return false;
    }

    // The configuration currently occupies almost 4 KB as JSON. ArduinoJson
    // also needs space for its object tree and duplicated strings.
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);

    if (error)
    {
        Log.printf("Error processing configuration: %s\r\n", error.c_str());
        file.close();
        return false;
    }

    if (configuration.General.Debug)
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
    strlcpy(configuration.Mqtt.Topics.HeatingValues, MQTT_Topics["HeatingValues"], sizeof(configuration.Mqtt.Topics.HeatingValues));             //"cerasmarter/heating/values"
    strlcpy(configuration.Mqtt.Topics.WaterValues, MQTT_Topics["WaterValues"], sizeof(configuration.Mqtt.Topics.WaterValues));                   //"cerasmarter/water/values"
    strlcpy(configuration.Mqtt.Topics.HeatingParameters, MQTT_Topics["HeatingParameters"], sizeof(configuration.Mqtt.Topics.HeatingParameters)); //"cerasmarter/heating/parameters"
    strlcpy(configuration.Mqtt.Topics.WaterParameters, MQTT_Topics["WaterParameters"], sizeof(configuration.Mqtt.Topics.WaterParameters));       //"cerasmarter/water/parameters"
    strlcpy(configuration.Mqtt.Topics.AuxiliaryValues, MQTT_Topics["AuxiliaryValues"], sizeof(configuration.Mqtt.Topics.AuxiliaryValues));          //"cerasmarter/auxiliary/parameters"
    strlcpy(configuration.Mqtt.Topics.Status, MQTT_Topics["Status"], sizeof(configuration.Mqtt.Topics.Status));                                  // "cerasmarter/status"
    strlcpy(configuration.Mqtt.Topics.StatusRequest, MQTT_Topics["StatusRequest"], sizeof(configuration.Mqtt.Topics.StatusRequest));             // "cerasmarter/status/get"
    strlcpy(configuration.Mqtt.Topics.Boost, MQTT_Topics["Boost"], sizeof(configuration.Mqtt.Topics.Boost));                                     // "cerasmarter/status/get"
    strlcpy(configuration.Mqtt.Topics.FastHeatup, MQTT_Topics["FastHeatup"], sizeof(configuration.Mqtt.Topics.FastHeatup));                      // "cerasmarter/status/get"

    JsonObject Features = doc["Features"];
    configuration.Features.HeatingParameters = Features["HeatingParameters"]; // true
    configuration.Features.WaterParameters = Features["WaterParameters"];     // false
    configuration.Features.AuxiliaryParameters = Features["AuxiliaryValues"];   // false
    configuration.Features.UseAuxiliaryOutsideTempReference = Features["OverrideOT"];

    JsonObject TimeSettings = doc["Time"];
    strlcpy(configuration.General.Timezone, TimeSettings["Timezone"], sizeof(configuration.General.Timezone)); // true
    const char *posixTimezone = TimeSettings["PosixTimezone"] | "CET-1CEST,M3.5.0,M10.5.0/3";
    strlcpy(configuration.General.PosixTimezone, posixTimezone, sizeof(configuration.General.PosixTimezone));

    JsonObject GeneralSettings = doc["General"];
    configuration.General.BusMessageTimeout = GeneralSettings["BusMessageTimeout"];
    configuration.General.SetpointOffDelaySeconds = GeneralSettings["SetpointOffDelaySeconds"] | configuration.General.SetpointOffDelaySeconds;
    configuration.General.Debug = GeneralSettings["Debug"];
    configuration.General.Sniffing = GeneralSettings["Sniffing"];
    if (configuration.General.SetpointOffDelaySeconds < 10 || configuration.General.SetpointOffDelaySeconds > 86400)
        configuration.General.SetpointOffDelaySeconds = 300;

    JsonObject FailSafeSettings = doc["FailSafe"];
    if (!FailSafeSettings.isNull())
    {
        configuration.FailSafe.Enabled = FailSafeSettings["Enabled"] | configuration.FailSafe.Enabled;
        configuration.FailSafe.CommandTimeoutSeconds = FailSafeSettings["CommandTimeoutSeconds"] | configuration.FailSafe.CommandTimeoutSeconds;
        configuration.FailSafe.StartHour = FailSafeSettings["StartHour"] | configuration.FailSafe.StartHour;
        configuration.FailSafe.StartMinute = FailSafeSettings["StartMinute"] | configuration.FailSafe.StartMinute;
        configuration.FailSafe.StopHour = FailSafeSettings["StopHour"] | configuration.FailSafe.StopHour;
        configuration.FailSafe.StopMinute = FailSafeSettings["StopMinute"] | configuration.FailSafe.StopMinute;
        configuration.FailSafe.HeatWhenTimeUnknown = FailSafeSettings["HeatWhenTimeUnknown"] | configuration.FailSafe.HeatWhenTimeUnknown;
        configuration.FailSafe.BasepointTemperature = FailSafeSettings["BasepointTemperature"] | configuration.FailSafe.BasepointTemperature;
        configuration.FailSafe.EndpointTemperature = FailSafeSettings["EndpointTemperature"] | configuration.FailSafe.EndpointTemperature;
        configuration.FailSafe.MinimumFeedTemperature = FailSafeSettings["MinimumFeedTemperature"] | configuration.FailSafe.MinimumFeedTemperature;
        configuration.FailSafe.MaximumFeedTemperature = FailSafeSettings["MaximumFeedTemperature"] | configuration.FailSafe.MaximumFeedTemperature;
    }
    if (configuration.FailSafe.CommandTimeoutSeconds < 10 || configuration.FailSafe.CommandTimeoutSeconds > 86400)
        configuration.FailSafe.CommandTimeoutSeconds = 300;
    configuration.FailSafe.StartHour = constrain(configuration.FailSafe.StartHour, 0, 23);
    configuration.FailSafe.StartMinute = constrain(configuration.FailSafe.StartMinute, 0, 59);
    configuration.FailSafe.StopHour = constrain(configuration.FailSafe.StopHour, 0, 23);
    configuration.FailSafe.StopMinute = constrain(configuration.FailSafe.StopMinute, 0, 59);
    if (!isfinite(configuration.FailSafe.BasepointTemperature))
        configuration.FailSafe.BasepointTemperature = -10;
    if (!isfinite(configuration.FailSafe.EndpointTemperature))
        configuration.FailSafe.EndpointTemperature = 31;
    if (!isfinite(configuration.FailSafe.MinimumFeedTemperature) || configuration.FailSafe.MinimumFeedTemperature < 0)
        configuration.FailSafe.MinimumFeedTemperature = 10;
    if (!isfinite(configuration.FailSafe.MaximumFeedTemperature) ||
        configuration.FailSafe.MaximumFeedTemperature < configuration.FailSafe.MinimumFeedTemperature ||
        configuration.FailSafe.MaximumFeedTemperature > 100)
        configuration.FailSafe.MaximumFeedTemperature = 55;

    JsonObject HomeAssistantSettings = doc["HomeAssistant"];
    configuration.HomeAssistant.Enabled = HomeAssistantSettings["Enabled"] | false;
    configuration.HomeAssistant.OffDelay = HomeAssistantSettings["OffDelay"] | 0;

    String deviceId = HomeAssistantSettings["DeviceId"].as<String>();
    if (deviceId.isEmpty())
        deviceId = configuration.Wifi.Hostname;
    if (deviceId.isEmpty())
        deviceId = "cerasmarter";
    deviceId.trim();
    deviceId.replace(" ", "_");
    deviceId.replace("/", "_");
    configuration.HomeAssistant.DeviceId = deviceId;

    String discoveryPrefix = HomeAssistantSettings["AutoDiscoveryPrefix"].as<String>();
    discoveryPrefix.trim();
    while (discoveryPrefix.endsWith("/"))
        discoveryPrefix.remove(discoveryPrefix.length() - 1);
    if (discoveryPrefix.isEmpty())
        discoveryPrefix = "homeassistant";
    configuration.HomeAssistant.AutoDiscoveryPrefix = discoveryPrefix;

    String temperatureUnit = HomeAssistantSettings["TempUnit"].as<String>();
    configuration.HomeAssistant.TempUnit = temperatureUnit.isEmpty() ? "°C" : temperatureUnit;
    configuration.HomeAssistant.StateTopic = "cerasmarter/" + configuration.HomeAssistant.DeviceId + "/";

    JsonObject Leds = doc["LEDs"];
    if (Leds["Wifi"].is<int>())
        configuration.LEDs.WifiLed = Leds["Wifi"];
    if (Leds["Status"].is<int>())
        configuration.LEDs.StatusLed = Leds["Status"];
    if (Leds["Mqtt"].is<int>())
        configuration.LEDs.MqttLed = Leds["Mqtt"];
    if (Leds["Heating"].is<int>())
        configuration.LEDs.HeatingLed = Leds["Heating"];

    JsonObject CAN = doc["CAN"];
    configuration.CanModuleConfig.CAN_Quartz = CAN["Quartz"];

    JsonObject CAN_Addresses = CAN["Addresses"];

    JsonObject CAN_Addresses_Controller = CAN_Addresses["Controller"];
    configuration.CanAddresses.General.FlameLit = convertHexString(CAN_Addresses_Controller["FlameStatus"].as<const char *>()); // "0x209"
    configuration.CanAddresses.General.Error = convertHexString(CAN_Addresses_Controller["Error"].as<const char *>());          // "0x206"
    configuration.CanAddresses.General.DateTime = convertHexString(CAN_Addresses_Controller["DateTime"].as<const char *>());    // "0x256"

    JsonObject CAN_Addresses_Heating = CAN_Addresses["Heating"];
    configuration.CanAddresses.Heating.FeedCurrent = convertHexString(CAN_Addresses_Heating["FeedCurrent"].as<const char *>());               // "0x201"
    configuration.CanAddresses.Heating.FeedMax = convertHexString(CAN_Addresses_Heating["FeedMax"].as<const char *>());                       // "0x200"
    configuration.CanAddresses.Heating.FeedSetpoint = convertHexString(CAN_Addresses_Heating["FeedSetpoint"].as<const char *>());             // "0x252"
    configuration.CanAddresses.Heating.OutsideTemperature = convertHexString(CAN_Addresses_Heating["OutsideTemperature"].as<const char *>()); // "0x207"
    configuration.CanAddresses.Heating.Pump = convertHexString(CAN_Addresses_Heating["Pump"].as<const char *>());                             // "0x20A"
    configuration.CanAddresses.Heating.Season = convertHexString(CAN_Addresses_Heating["Season"].as<const char *>());                         // "0x20C"
    configuration.CanAddresses.Heating.Operation = convertHexString(CAN_Addresses_Heating["Operation"].as<const char *>());                   // "0x250"
    configuration.CanAddresses.Heating.Power = convertHexString(CAN_Addresses_Heating["Power"].as<const char *>());                           // "0x251"
    configuration.CanAddresses.Heating.Mode = convertHexString(CAN_Addresses_Heating["Mode"].as<const char *>());                             // "0x258"
    configuration.CanAddresses.Heating.Economy = convertHexString(CAN_Addresses_Heating["Economy"].as<const char *>());                       // "0x253"

    JsonObject CAN_Addresses_HotWater = CAN_Addresses["HotWater"];
    configuration.CanAddresses.HotWater.SetpointTemperature = convertHexString(CAN_Addresses_HotWater["SetpointTemperature"].as<const char *>()); // "0x203"
    configuration.CanAddresses.HotWater.MaxTemperature = convertHexString(CAN_Addresses_HotWater["MaxTemperature"].as<const char *>());           // "0x204"
    configuration.CanAddresses.HotWater.CurrentTemperature = convertHexString(CAN_Addresses_HotWater["CurrentTemperature"].as<const char *>());   // "0x205"
    configuration.CanAddresses.HotWater.Now = convertHexString(CAN_Addresses_HotWater["Now"].as<const char *>());                                 // "0x254"
    configuration.CanAddresses.HotWater.BufferOperation = convertHexString(CAN_Addresses_HotWater["BufferOperation"].as<const char *>());         // "0x20B"

    configuration
        .CanAddresses
        .HotWater
        .ContinousFlowSetpointTemperature = convertHexString(CAN_Addresses_HotWater["ContinousFlow"]["SetpointTemperature"].as<const char *>()); // "0x255"

    JsonObject CAN_Addresses_MixedCircuit = CAN_Addresses["MixedCircuit"];
    configuration.CanAddresses.MixedCircuit.Pump = convertHexString(CAN_Addresses_MixedCircuit["Pump"].as<const char *>());                 // "0x404"
    configuration.CanAddresses.MixedCircuit.FeedSetpoint = convertHexString(CAN_Addresses_MixedCircuit["FeedSetpoint"].as<const char *>()); // "0x405"
    configuration.CanAddresses.MixedCircuit.FeedCurrent = convertHexString(CAN_Addresses_MixedCircuit["FeedCurrent"].as<const char *>());   // "0x440"
    configuration.CanAddresses.MixedCircuit.Economy = convertHexString(CAN_Addresses_MixedCircuit["Economy"].as<const char *>());           // "0x407"

    int curSensor = 0;
    bool tempReferenceSensorSet = false;

    JsonArray sensorsArray = doc["AuxiliarySensors"]["Sensors"].as<JsonArray>();

    const size_t sensorCount = sensorsArray.size();

    // Resize the Temperature array
    ceraValues.Auxiliary.Temperatures = (float *)malloc(sensorCount * sizeof(float));

    // Set initial values to zero.
    for (size_t i = 0; i < sensorCount; i++)
    {
        ceraValues.Auxiliary.Temperatures[i] = 0.0F;
    }

    // Init Sensors: This might cause trouble if too many sensorsArray are added... we'll have to see where this is going.
    configuration.TemperatureSensors.Sensors = (Sensor *)malloc(sensorCount * sizeof(Sensor));

    // Set the amount of sensorsArray
    configuration.TemperatureSensors.SensorCount = sensorCount;

    for (JsonObject AuxiliarySensors_Sensor : sensorsArray)
    {
        Sensor newSensor{};
        strlcpy(newSensor.Label, AuxiliarySensors_Sensor["Label"], sizeof(newSensor.Label)); // true
        newSensor.UseAsReturnValueReference = AuxiliarySensors_Sensor["IsReturnValue"].as<bool>();
        JsonArray AuxiliarySensors_Sensor_Address = AuxiliarySensors_Sensor["Address"];
        int i = 0;
        for (JsonVariant value : AuxiliarySensors_Sensor_Address)
        {
            byte addrByte = strtoul(value.as<const char *>(), NULL, 16);
            newSensor.Address[i++] = addrByte;
        }
        configuration.TemperatureSensors.Sensors[curSensor++] = newSensor;
        if (configuration.General.Debug)
        {
            if (newSensor.UseAsReturnValueReference && tempReferenceSensorSet)
            {
                Log.printf("WARN: Sensor #%i is set as temperature reference but another sensor has been already set.", curSensor);
            }
            if (newSensor.UseAsReturnValueReference)
            {
                Log.println("INFO: The following sensor will be used as a return temperature reference.");
                tempReferenceSensorSet = true;
            }
            Log.printf("Added Sensor #%i with Label '%s'\r\n", curSensor, newSensor.Label);
        }
    }
    file.close();
    String backupError;
    if (!PersistConfigurationBackup(backupError))
        Log.println("Warning: configuration loaded, but its persistent backup could not be refreshed: " + backupError);
    return true;
}

bool WriteConfiguration()
{
    // An uploaded configuration is authoritative until the device reboots.
    // Refuse to overwrite it with the stale runtime copy in the meantime.
    if (configurationUploadPending)
    {
        Log.println("Configuration save skipped: an uploaded configuration is pending reboot.");
        return false;
    }

    JsonDocument doc;

    JsonObject Wifi = doc["Wifi"].to<JsonObject>();
    Wifi["SSID"] = configuration.Wifi.SSID;
    Wifi["Password"] = configuration.Wifi.Password;
    Wifi["Hostname"] = configuration.Wifi.Hostname;

    JsonObject MQTT = doc["MQTT"].to<JsonObject>();
    MQTT["Server"] = configuration.Mqtt.Server;
    MQTT["Port"] = configuration.Mqtt.Port;
    MQTT["User"] = configuration.Mqtt.User;
    MQTT["Password"] = configuration.Mqtt.Password;

    JsonObject MQTT_Topics = MQTT["Topics"].to<JsonObject>();
    MQTT_Topics["HeatingValues"] = configuration.Mqtt.Topics.HeatingValues;
    MQTT_Topics["HeatingParameters"] = configuration.Mqtt.Topics.HeatingParameters;
    MQTT_Topics["WaterValues"] = configuration.Mqtt.Topics.WaterValues;
    MQTT_Topics["WaterParameters"] = configuration.Mqtt.Topics.WaterParameters;
    MQTT_Topics["AuxiliaryValues"] = configuration.Mqtt.Topics.AuxiliaryValues;
    MQTT_Topics["Status"] = configuration.Mqtt.Topics.Status;
    MQTT_Topics["StatusRequest"] = configuration.Mqtt.Topics.StatusRequest;
    MQTT_Topics["Boost"] = configuration.Mqtt.Topics.Boost;
    MQTT_Topics["FastHeatup"] = configuration.Mqtt.Topics.FastHeatup;

    JsonObject Features = doc["Features"].to<JsonObject>();
    Features["HeatingParameters"] = configuration.Features.HeatingParameters;
    Features["WaterParameters"] = configuration.Features.WaterParameters;
    Features["AuxiliaryValues"] = configuration.Features.AuxiliaryParameters;
    Features["OverrideOT"] = configuration.Features.UseAuxiliaryOutsideTempReference;

    doc["Time"]["Timezone"] = configuration.General.Timezone;
    doc["Time"]["PosixTimezone"] = configuration.General.PosixTimezone;

    JsonObject General = doc["General"].to<JsonObject>();
    General["BusMessageTimeout"] = configuration.General.BusMessageTimeout;
    General["SetpointOffDelaySeconds"] = configuration.General.SetpointOffDelaySeconds;
    General["Debug"] = configuration.General.Debug;
    General["Sniffing"] = configuration.General.Sniffing;

    JsonObject FailSafe = doc["FailSafe"].to<JsonObject>();
    FailSafe["Enabled"] = configuration.FailSafe.Enabled;
    FailSafe["CommandTimeoutSeconds"] = configuration.FailSafe.CommandTimeoutSeconds;
    FailSafe["StartHour"] = configuration.FailSafe.StartHour;
    FailSafe["StartMinute"] = configuration.FailSafe.StartMinute;
    FailSafe["StopHour"] = configuration.FailSafe.StopHour;
    FailSafe["StopMinute"] = configuration.FailSafe.StopMinute;
    FailSafe["HeatWhenTimeUnknown"] = configuration.FailSafe.HeatWhenTimeUnknown;
    FailSafe["BasepointTemperature"] = configuration.FailSafe.BasepointTemperature;
    FailSafe["EndpointTemperature"] = configuration.FailSafe.EndpointTemperature;
    FailSafe["MinimumFeedTemperature"] = configuration.FailSafe.MinimumFeedTemperature;
    FailSafe["MaximumFeedTemperature"] = configuration.FailSafe.MaximumFeedTemperature;

    JsonObject HomeAssistant = doc["HomeAssistant"].to<JsonObject>();
    HomeAssistant["AutoDiscoveryPrefix"] = configuration.HomeAssistant.AutoDiscoveryPrefix;
    HomeAssistant["OffDelay"] = configuration.HomeAssistant.OffDelay;
    HomeAssistant["Enabled"] = configuration.HomeAssistant.Enabled;
    HomeAssistant["DeviceId"] = configuration.HomeAssistant.DeviceId;
    HomeAssistant["TempUnit"] = configuration.HomeAssistant.TempUnit;

    JsonObject CAN = doc["CAN"].to<JsonObject>();
    CAN["Quartz"] = configuration.CanModuleConfig.CAN_Quartz;

    JsonObject CAN_Addresses = CAN["Addresses"].to<JsonObject>();

    JsonObject CAN_Addresses_Controller = CAN_Addresses["Controller"].to<JsonObject>();
    CAN_Addresses_Controller["FlameStatus"] = IntToHex(configuration.CanAddresses.General.FlameLit);
    CAN_Addresses_Controller["Error"] = IntToHex(configuration.CanAddresses.General.Error);
    CAN_Addresses_Controller["DateTime"] = IntToHex(configuration.CanAddresses.General.DateTime);

    JsonObject CAN_Addresses_Heating = CAN_Addresses["Heating"].to<JsonObject>();
    CAN_Addresses_Heating["FeedCurrent"] = IntToHex(configuration.CanAddresses.Heating.FeedCurrent);
    CAN_Addresses_Heating["FeedMax"] = IntToHex(configuration.CanAddresses.Heating.FeedMax);
    CAN_Addresses_Heating["FeedSetpoint"] = IntToHex(configuration.CanAddresses.Heating.FeedSetpoint);
    CAN_Addresses_Heating["OutsideTemperature"] = IntToHex(configuration.CanAddresses.Heating.OutsideTemperature);
    CAN_Addresses_Heating["Pump"] = IntToHex(configuration.CanAddresses.Heating.Pump);
    CAN_Addresses_Heating["Season"] = IntToHex(configuration.CanAddresses.Heating.Season);
    CAN_Addresses_Heating["Operation"] = IntToHex(configuration.CanAddresses.Heating.Operation);
    CAN_Addresses_Heating["Power"] = IntToHex(configuration.CanAddresses.Heating.Power);
    CAN_Addresses_Heating["Mode"] = IntToHex(configuration.CanAddresses.Heating.Mode);
    CAN_Addresses_Heating["Economy"] = IntToHex(configuration.CanAddresses.Heating.Economy);

    JsonObject CAN_Addresses_HotWater = CAN_Addresses["HotWater"].to<JsonObject>();
    CAN_Addresses_HotWater["SetpointTemperature"] = IntToHex(configuration.CanAddresses.HotWater.SetpointTemperature);
    CAN_Addresses_HotWater["MaxTemperature"] = IntToHex(configuration.CanAddresses.HotWater.MaxTemperature);
    CAN_Addresses_HotWater["CurrentTemperature"] = IntToHex(configuration.CanAddresses.HotWater.CurrentTemperature);
    CAN_Addresses_HotWater["Now"] = IntToHex(configuration.CanAddresses.HotWater.Now);
    CAN_Addresses_HotWater["BufferOperation"] = IntToHex(configuration.CanAddresses.HotWater.BufferOperation);
    CAN_Addresses_HotWater["ContinousFlow"]["SetpointTemperature"] = IntToHex(configuration.CanAddresses.HotWater.ContinousFlowSetpointTemperature);

    JsonObject CAN_Addresses_MixedCircuit = CAN_Addresses["MixedCircuit"].to<JsonObject>();
    CAN_Addresses_MixedCircuit["Pump"] = IntToHex(configuration.CanAddresses.MixedCircuit.Pump);
    CAN_Addresses_MixedCircuit["FeedSetpoint"] = IntToHex(configuration.CanAddresses.MixedCircuit.FeedSetpoint);
    CAN_Addresses_MixedCircuit["FeedCurrent"] = IntToHex(configuration.CanAddresses.MixedCircuit.FeedCurrent);
    CAN_Addresses_MixedCircuit["Economy"] = IntToHex(configuration.CanAddresses.MixedCircuit.Economy);

    JsonArray AuxiliarySensors_Sensors = doc["AuxiliarySensors"]["Sensors"].to<JsonArray>();

    for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
    {
        JsonObject sensorEntry = AuxiliarySensors_Sensors.add<JsonObject>();
        Sensor curSensor = configuration.TemperatureSensors.Sensors[i];
        sensorEntry["Label"] = curSensor.Label;
        sensorEntry["IsReturnValue"] = curSensor.UseAsReturnValueReference;
        JsonArray address = sensorEntry["Address"].to<JsonArray>();
        // Device address has a fixed size of 8
        for (unsigned char curAddress : curSensor.Address)
        {
            char byteBuf[5];
            sprintf(byteBuf, "0x%.2X", curAddress);
            address.add(byteBuf);
        }
    }

    JsonObject LEDs = doc["LEDs"].to<JsonObject>();
    LEDs["Wifi"] = configuration.LEDs.WifiLed;
    LEDs["Status"] = configuration.LEDs.StatusLed;
    LEDs["Mqtt"] = configuration.LEDs.MqttLed;
    LEDs["Heating"] = configuration.LEDs.HeatingLed;

    if (doc.overflowed())
    {
        Log.println("Configuration could not be saved: JSON document overflowed.");
        return false;
    }

    File file = LittleFS.open(temporaryConfigFileName, FILE_WRITE, true);
    if (!file)
    {
        Log.println("Configuration could not be saved: temporary file could not be opened.");
        return false;
    }

    const size_t bytesWritten = serializeJsonPretty(doc, file);
    file.flush();
    file.close();

    if (bytesWritten == 0)
    {
        LittleFS.remove(temporaryConfigFileName);
        Log.println("Configuration could not be saved: serialization failed.");
        return false;
    }

    // Keep the previous file recoverable until the complete replacement has
    // been committed.
    LittleFS.remove(backupConfigFileName);
    if (LittleFS.exists(configFileName) && !LittleFS.rename(configFileName, backupConfigFileName))
    {
        LittleFS.remove(temporaryConfigFileName);
        Log.println("Configuration could not be saved: current file could not be backed up.");
        return false;
    }

    if (!LittleFS.rename(temporaryConfigFileName, configFileName))
    {
        LittleFS.rename(backupConfigFileName, configFileName);
        Log.println("Configuration could not be saved: temporary file could not be committed.");
        return false;
    }

    // Verify the committed file before discarding the known-good backup.
    File committedFile = LittleFS.open(configFileName, FILE_READ);
    JsonDocument committedDoc;
    DeserializationError verificationError = deserializeJson(committedDoc, committedFile);
    committedFile.close();
    if (verificationError || !committedDoc.is<JsonObject>())
    {
        LittleFS.remove(configFileName);
        LittleFS.rename(backupConfigFileName, configFileName);
        Log.printf("Configuration verification failed: %s\r\n", verificationError.c_str());
        return false;
    }

    LittleFS.remove(backupConfigFileName);
    String backupError;
    if (!PersistConfigurationBackup(backupError))
        Log.println("Warning: configuration saved, but its persistent backup could not be refreshed: " + backupError);
    return true;
}
