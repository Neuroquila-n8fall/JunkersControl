#include <ha_autodiscovery.h>

#include <ArduinoJson.h>
#include <configuration.h>
#include <failsafe.h>
#include <heating.h>
#include <main.h>
#include <mqtt.h>

namespace
{
String availabilityTopic()
{
    return configuration.HomeAssistant.StateTopic + "availability";
}

String discoveryTopic()
{
    return configuration.HomeAssistant.AutoDiscoveryPrefix + "/device/" +
           configuration.HomeAssistant.DeviceId + "/config";
}

String homeAssistantStatusTopic()
{
    return configuration.HomeAssistant.AutoDiscoveryPrefix + "/status";
}

String sanitizeId(String value)
{
    value.toLowerCase();
    for (size_t i = 0; i < value.length(); i++)
    {
        const char c = value.charAt(i);
        if (!isAlphaNumeric(c) && c != '_' && c != '-')
            value.setCharAt(i, '_');
    }
    return value;
}

String softwareVersion()
{
    String version = JC_STRINGIFY(VERSION);
    version.replace("\"", "");
    return version;
}

String stateTopic(const String &category)
{
    return configuration.HomeAssistant.StateTopic + category + "/state";
}

String commandTopic(const String &category, const String &key)
{
    return configuration.HomeAssistant.StateTopic + category + "/" + key + "/set";
}

String uniqueId(const String &componentId)
{
    return sanitizeId(configuration.HomeAssistant.DeviceId + "_" + componentId);
}

JsonObject addComponent(JsonObject components, const String &componentId, const char *platform, const String &name)
{
    JsonObject component = components[componentId].to<JsonObject>();
    component["p"] = platform;
    component["name"] = name;
    component["unique_id"] = uniqueId(componentId);
    return component;
}

void addSensor(JsonObject components,
               const String &componentId,
               const String &name,
               const String &category,
               const String &valueTemplate,
               const char *deviceClass = nullptr,
               const String &unit = "",
               const char *icon = nullptr)
{
    JsonObject sensor = addComponent(components, componentId, "sensor", name);
    sensor["state_topic"] = stateTopic(category);
    sensor["value_template"] = valueTemplate;
    if (deviceClass != nullptr)
        sensor["device_class"] = deviceClass;
    if (!unit.isEmpty())
        sensor["unit_of_measurement"] = unit;
    if (icon != nullptr)
        sensor["icon"] = icon;
    if (deviceClass != nullptr && strcmp(deviceClass, "temperature") == 0)
        sensor["state_class"] = "measurement";
}

void addDiagnosticSensor(JsonObject components,
                         const String &componentId,
                         const String &name,
                         const String &valueTemplate,
                         const char *deviceClass,
                         const String &unit,
                         const char *icon,
                         const char *stateClass = nullptr)
{
    addSensor(components, componentId, name, "General", valueTemplate, deviceClass, unit, icon);
    JsonObject sensor = components[componentId].as<JsonObject>();
    sensor["entity_category"] = "diagnostic";
    if (stateClass != nullptr)
        sensor["state_class"] = stateClass;
}

void addBinarySensor(JsonObject components,
                     const String &componentId,
                     const String &name,
                     const String &category,
                     const String &valueTemplate,
                     const char *deviceClass = nullptr,
                     const char *icon = nullptr,
                     const char *entityCategory = nullptr)
{
    JsonObject sensor = addComponent(components, componentId, "binary_sensor", name);
    sensor["state_topic"] = stateTopic(category);
    sensor["value_template"] = valueTemplate;
    sensor["payload_on"] = "true";
    sensor["payload_off"] = "false";
    if (deviceClass != nullptr)
        sensor["device_class"] = deviceClass;
    if (icon != nullptr)
        sensor["icon"] = icon;
    if (entityCategory != nullptr)
        sensor["entity_category"] = entityCategory;
    if (configuration.HomeAssistant.OffDelay > 0)
        sensor["off_delay"] = configuration.HomeAssistant.OffDelay;
}

void addNumber(JsonObject components,
               const String &componentId,
               const String &name,
               const String &category,
               const String &key,
               const String &valueTemplate,
               double minimum,
               double maximum,
               double step,
               const String &unit = "",
               const char *mode = "slider",
               const char *icon = nullptr)
{
    JsonObject number = addComponent(components, componentId, "number", name);
    number["state_topic"] = stateTopic(category);
    number["command_topic"] = commandTopic(category, key);
    number["value_template"] = valueTemplate;
    number["min"] = minimum;
    number["max"] = maximum;
    number["step"] = step;
    number["mode"] = mode;
    if (!unit.isEmpty())
        number["unit_of_measurement"] = unit;
    if (icon != nullptr)
        number["icon"] = icon;

    client.subscribe(number["command_topic"].as<const char *>());
}

void addSwitch(JsonObject components,
               const String &componentId,
               const String &name,
               const String &category,
               const String &key,
               const String &valueTemplate,
               const char *icon = nullptr)
{
    JsonObject control = addComponent(components, componentId, "switch", name);
    control["state_topic"] = stateTopic(category);
    control["command_topic"] = commandTopic(category, key);
    control["value_template"] = valueTemplate;
    control["payload_on"] = "true";
    control["payload_off"] = "false";
    control["state_on"] = "true";
    control["state_off"] = "false";
    control["optimistic"] = false;
    if (icon != nullptr)
        control["icon"] = icon;

    client.subscribe(control["command_topic"].as<const char *>());
}

String escapeTemplateKey(String value)
{
    value.replace("\\", "\\\\");
    value.replace("'", "\\'");
    return value;
}

void addCoreComponents(JsonObject components)
{
    const String temperatureUnit = configuration.HomeAssistant.TempUnit;

    addSensor(components, "general_error", "Error", "General",
              "{{ value_json.General.Error | int(default=0) }}", nullptr, "", "mdi:alert-circle-outline");

    addDiagnosticSensor(components, "diagnostic_free_heap", "Free Heap",
                        "{{ value_json.General.FreeHeap | int(default=0) }}", "data_size", "B",
                        "mdi:memory", "measurement");
    addDiagnosticSensor(components, "diagnostic_heap_size", "Heap Size",
                        "{{ value_json.General.HeapSize | int(default=0) }}", "data_size", "B",
                        "mdi:memory", "measurement");
    addDiagnosticSensor(components, "diagnostic_filesystem_used", "Filesystem Used",
                        "{{ value_json.General.FilesystemUsed | int(default=0) }}", "data_size", "B",
                        "mdi:database", "measurement");
    addDiagnosticSensor(components, "diagnostic_filesystem_size", "Filesystem Size",
                        "{{ value_json.General.FilesystemSize | int(default=0) }}", "data_size", "B",
                        "mdi:database-outline", "measurement");
    addDiagnosticSensor(components, "diagnostic_flash_size", "Flash Size",
                        "{{ value_json.General.FlashSize | int(default=0) }}", "data_size", "B",
                        "mdi:chip", "measurement");
    addDiagnosticSensor(components, "diagnostic_chip_model", "Chip Model",
                        "{{ value_json.General.ChipModel | default('unknown') }}", nullptr, "",
                        "mdi:chip");
    addDiagnosticSensor(components, "diagnostic_chip_revision", "Chip Revision",
                        "{{ value_json.General.ChipRevision | default('unknown') }}", nullptr, "",
                        "mdi:counter");
    addDiagnosticSensor(components, "diagnostic_cpu_cores", "CPU Cores",
                        "{{ value_json.General.CpuCores | int(default=0) }}", nullptr, "",
                        "mdi:cpu-32-bit");
    addDiagnosticSensor(components, "diagnostic_cpu_frequency", "CPU Frequency",
                        "{{ value_json.General.CpuFrequency | int(default=0) }}", "frequency", "MHz",
                        "mdi:speedometer", "measurement");

    addSensor(components, "heating_feed_current", "Current Feed Temperature", "Heating",
              "{{ value_json.Heating.FeedCurrent | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:thermometer-water");
    addSensor(components, "heating_feed_maximum", "Maximum Feed Temperature", "Heating",
              "{{ value_json.Heating.FeedMaximum | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:thermometer-high");
    addSensor(components, "heating_feed_setpoint", "Feed Setpoint Temperature", "Heating",
              "{{ value_json.Heating.FeedSetpoint | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:thermometer-check");
    addSensor(components, "heating_outside", "Outside Temperature", "Heating",
              "{{ value_json.Heating.Outside | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:sun-thermometer-outline");
    addSensor(components, "heating_boost_time_left", "Boost Time Remaining", "Heating",
              "{{ value_json.Heating.BoostTimeLeft | int(default=0) }}", "duration", "s",
              "mdi:timer-sand");

    addSensor(components, "water_maximum", "Maximum Water Temperature", "Water",
              "{{ value_json.Water.Maximum | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:thermometer-high");
    addSensor(components, "water_current", "Current Water Temperature", "Water",
              "{{ value_json.Water.Current | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:water-thermometer");
    addSensor(components, "water_setpoint", "Water Setpoint Temperature", "Water",
              "{{ value_json.Water.Setpoint | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:thermometer-check");
    addSensor(components, "water_continuous_flow_setpoint", "Continuous Flow Setpoint", "Water",
              "{{ value_json.Water.CFSetpoint | float(default=0) }}", "temperature", temperatureUnit,
              "mdi:water-sync");

    addBinarySensor(components, "general_gas_burner", "Flame Lit", "General",
                    "{{ value_json.General.GasBurner }}", nullptr, "mdi:fire");
    addBinarySensor(components, "heating_pump", "Heating Pump", "Heating",
                    "{{ value_json.Heating.Pump }}", "running", "mdi:pump");
    addBinarySensor(components, "heating_season", "Heating Season", "Heating",
                    "{{ value_json.Heating.Season }}", nullptr, "mdi:radiator");
    addBinarySensor(components, "heating_working", "Heating Operation", "Heating",
                    "{{ value_json.Heating.Working }}", "running", "mdi:heating-coil");
    addBinarySensor(components, "water_now", "Hot Water Now", "Water",
                    "{{ value_json.Water.Now }}", "running", "mdi:water-boiler");
    addBinarySensor(components, "water_buffer", "Hot Water Buffer Mode", "Water",
                    "{{ value_json.Water.Buffer }}", "running", "mdi:water-boiler-auto");

    addNumber(components, "heating_requested_setpoint", "Requested Feed Setpoint", "Heating", "FeedSetpoint",
              "{{ value_json.Heating.RequestedFeedSetpoint | float(default=0) }}", 0, 100, 0.5, temperatureUnit,
              "slider", "mdi:thermometer-chevron-up");
    addNumber(components, "heating_curve_basepoint", "Heating Curve Basepoint", "Heating", "FeedBaseSetpoint",
              "{{ value_json.Heating.FeedBaseSetpoint | float(default=0) }}", -50, 50, 0.5, temperatureUnit,
              "box", "mdi:chart-bell-curve-cumulative");
    addNumber(components, "heating_curve_cutoff", "Heating Curve Cutoff", "Heating", "FeedCutOff",
              "{{ value_json.Heating.FeedCutOff | float(default=0) }}", -50, 50, 0.5, temperatureUnit,
              "box", "mdi:chart-bell-curve");
    addNumber(components, "heating_feed_minimum", "Minimum Feed Temperature", "Heating", "FeedMinimum",
              "{{ value_json.Heating.FeedMinimum | float(default=0) }}", 0, 100, 0.5, temperatureUnit,
              "slider", "mdi:thermometer-low");
    addNumber(components, "heating_manual_adaptation", "Manual Adaptation", "Heating", "Adaption",
              "{{ value_json.Heating.Adaption | float(default=0) }}", -30, 30, 0.5, temperatureUnit,
              "box", "mdi:tune-variant");
    addNumber(components, "heating_auxiliary_temperature", "Auxiliary Outside Temperature", "Heating", "AuxiliaryTemperature",
              "{{ value_json.Heating.AuxiliaryTemperature | float(default=0) }}", -50, 100, 0.1, temperatureUnit,
              "box", "mdi:sun-thermometer");
    addNumber(components, "heating_boost_duration", "Boost Duration", "Heating", "OnDemandBoostDuration",
              "{{ value_json.Heating.OnDemandBoostDuration | int(default=0) }}", 0, 86400, 1, "s", "box",
              "mdi:timer-outline");
    addNumber(components, "heating_room_reference", "Room Reference Temperature", "Heating", "AmbientTemperature",
              "{{ value_json.Heating.AmbientTemperature | float(default=0) }}", -50, 100, 0.1, temperatureUnit,
              "slider", "mdi:home-thermometer");
    addNumber(components, "heating_target_ambient", "Target Ambient Temperature", "Heating", "TargetAmbientTemperature",
              "{{ value_json.Heating.TargetAmbientTemperature | float(default=0) }}", 5, 35, 0.1, temperatureUnit,
              "slider", "mdi:home-thermometer-outline");
    addNumber(components, "heating_valve_opening", "Valve Opening", "Heating", "ValveScalingOpening",
              "{{ value_json.Heating.ValveScalingOpening | int(default=0) }}", 0, 100, 1, "%",
              "slider", "mdi:valve");
    addNumber(components, "heating_valve_max_opening", "Maximum Valve Opening", "Heating", "ValveScalingMaxOpening",
              "{{ value_json.Heating.ValveScalingMaxOpening | int(default=0) }}", 1, 100, 1, "%",
              "slider", "mdi:valve-open");
    addSwitch(components, "heating_enabled", "Heating Enabled", "Heating", "Enabled",
              "{{ value_json.Heating.Enabled }}", "mdi:radiator");
    addSwitch(components, "heating_override_setpoint", "Use Requested Feed Setpoint", "Heating", "OverrideSetpoint",
              "{{ value_json.Heating.OverrideSetpoint }}", "mdi:thermometer-lock");
    addSwitch(components, "heating_dynamic_adaptation", "Dynamic Adaptation", "Heating", "DynamicAdaption",
              "{{ value_json.Heating.DynamicAdaption }}", "mdi:auto-fix");
    addSwitch(components, "heating_valve_scaling", "Valve Scaling", "Heating", "ValveScaling",
              "{{ value_json.Heating.ValveScaling }}", "mdi:valve");
    addSwitch(components, "heating_boost", "Heating Boost", "Heating", "Boost",
              "{{ value_json.Heating.Boost }}", "mdi:fire-circle");
    addSwitch(components, "heating_fast_heatup", "Fast Heatup", "Heating", "FastHeatup",
              "{{ value_json.Heating.FastHeatup }}", "mdi:heat-wave");
}

void addAuxiliaryComponents(JsonObject components)
{
    for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
    {
        const String label = configuration.TemperatureSensors.Sensors[i].Label;
        const String id = sanitizeId(label);
        const String templateKey = escapeTemplateKey(label);

        addSensor(components, "auxiliary_" + id + "_temperature", label + " Temperature", "Auxiliary",
                  "{{ value_json.Auxiliary['" + templateKey + "'].Temperature | float(default=0) }}",
                  "temperature", configuration.HomeAssistant.TempUnit, "mdi:thermometer-probe");
        addBinarySensor(components, "auxiliary_" + id + "_reachable", label + " Reachable", "Auxiliary",
                        "{{ value_json.Auxiliary['" + templateKey + "'].Reachable }}", "connectivity",
                        "mdi:lan-connect", "diagnostic");
    }
}

bool parseNumber(const String &payload, double &value)
{
    char *end = nullptr;
    value = strtod(payload.c_str(), &end);
    return end != payload.c_str() && *end == '\0' && isfinite(value);
}

bool parseBoolean(const String &payload, bool &value)
{
    String normalized = payload;
    normalized.toLowerCase();
    if (normalized == "true" || normalized == "on" || normalized == "1")
    {
        value = true;
        return true;
    }
    if (normalized == "false" || normalized == "off" || normalized == "0")
    {
        value = false;
        return true;
    }
    return false;
}

bool isHeatingBooleanKey(const String &key)
{
    return key == "Enabled" || key == "ValveScaling" || key == "DynamicAdaption" ||
           key == "OverrideSetpoint" || key == "Boost" || key == "FastHeatup";
}

bool isHeatingNumberKey(const String &key)
{
    return key == "FeedSetpoint" || key == "FeedBaseSetpoint" || key == "FeedCutOff" ||
           key == "FeedMinimum" || key == "AuxiliaryTemperature" || key == "AmbientTemperature" ||
           key == "TargetAmbientTemperature" || key == "Adaption" ||
           key == "ValveScalingMaxOpening" || key == "ValveScalingOpening" ||
           key == "OnDemandBoostDuration";
}

bool handleHomeAssistantCommand(const String &relativeTopic, const String &payload)
{
    if (!relativeTopic.endsWith("/set"))
        return false;

    const int separator = relativeTopic.indexOf('/');
    if (separator <= 0)
        return false;

    const String category = relativeTopic.substring(0, separator);
    String key = relativeTopic.substring(separator + 1, relativeTopic.length() - 4);

    if (category == "Water" && key == "Setpoint")
    {
        double value = 0;
        if (!parseNumber(payload, value))
        {
            Log.printf("Ignoring invalid HA number payload on %s\r\n", relativeTopic.c_str());
            return true;
        }

        JsonDocument command;
        command["Setpoint"] = value;
        ApplyHotWaterCommand(command.as<JsonVariantConst>(), true);
        Log.printf("Applied MQTT command: %s\r\n", relativeTopic.c_str());
        return true;
    }

    if (category != "Heating")
        return false;

    // Accept command topics from the first HA implementation while publishing
    // canonical command names in current discovery documents.
    if (key == "Setpoint")
        key = "FeedSetpoint";
    else if (key == "BoostDuration")
        key = "OnDemandBoostDuration";
    else if (key == "RoomReferenceT")
        key = "AmbientTemperature";

    JsonDocument command;
    if (isHeatingBooleanKey(key))
    {
        bool value = false;
        if (!parseBoolean(payload, value))
        {
            Log.printf("Ignoring invalid HA switch payload on %s\r\n", relativeTopic.c_str());
            return true;
        }
        command[key] = value;
    }
    else if (isHeatingNumberKey(key))
    {
        double value = 0;
        if (!parseNumber(payload, value))
        {
            Log.printf("Ignoring invalid HA number payload on %s\r\n", relativeTopic.c_str());
            return true;
        }
        command[key] = value;
    }
    else
        return false;

    ApplyHeatingCommand(command.as<JsonVariantConst>(), true);
    Log.printf("Applied MQTT command: %s\r\n", relativeTopic.c_str());
    return true;
}
} // namespace

void PublishHomeAssistantAvailability(bool online)
{
    if (MUTE_MQTT == 1 || !configuration.HomeAssistant.Enabled || !client.connected())
        return;

    const String topic = availabilityTopic();
    client.publish(topic.c_str(), online ? "online" : "offline", true);
}

void SetupHomeAssistantDiscovery()
{
    if (MUTE_MQTT == 1 || !configuration.HomeAssistant.Enabled || !client.connected())
        return;

    client.subscribe(homeAssistantStatusTopic().c_str());

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    JsonObject device = root["dev"].to<JsonObject>();
    device["ids"] = configuration.HomeAssistant.DeviceId;
    device["name"] = configuration.HomeAssistant.DeviceId;
    device["mf"] = "Cerasmarter";
    device["mdl"] = "Cerasmart-er";
    device["sw"] = softwareVersion();

    JsonObject origin = root["o"].to<JsonObject>();
    origin["name"] = "Cerasmarter";
    origin["sw"] = softwareVersion();
    origin["url"] = "https://github.com/Neuroquila-n8fall/JunkersControl";

    root["availability_topic"] = availabilityTopic();
    root["payload_available"] = "online";
    root["payload_not_available"] = "offline";
    root["qos"] = 0;

    JsonObject components = root["cmps"].to<JsonObject>();
    addCoreComponents(components);
    addAuxiliaryComponents(components);

    String payload;
    serializeJson(doc, payload);
    const String topic = discoveryTopic();
    const bool published = client.publish(topic.c_str(), payload.c_str(), true);
    PublishHomeAssistantAvailability(true);

    Log.printf("Home Assistant device discovery %s (%u components, %u bytes).\r\n",
               published ? "published" : "failed",
               static_cast<unsigned int>(components.size()),
               static_cast<unsigned int>(payload.length()));
}

bool HandleHomeAssistantMessage(const char *topic, const String &payload)
{
    if (!configuration.HomeAssistant.Enabled)
        return false;

    if (homeAssistantStatusTopic() == topic)
    {
        if (payload == "online")
            SetupHomeAssistantDiscovery();
        return true;
    }

    const String statePrefix = configuration.HomeAssistant.StateTopic;
    const String messageTopic = topic;
    if (!messageTopic.startsWith(statePrefix) || !messageTopic.endsWith("/set"))
        return false;

    return handleHomeAssistantCommand(messageTopic.substring(statePrefix.length()), payload);
}
