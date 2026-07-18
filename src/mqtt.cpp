#include <Arduino.h>
#include <mqtt.h>
#include <configuration.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>
#include <ArduinoJson.h>
#include <ha_autodiscovery.h>
#include <failsafe.h>

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Client (uses Wifi Client)
//——————————————————————————————————————————————————————————————————————————————
PubSubClient client(espClient);

CommandedValues commandedValues;
String TopicBuf;
String PayloadBuf;
static unsigned long lastMqttAttempt = 0;
static const unsigned long mqttRetryInterval = 30000;

bool ApplyHeatingCommand(JsonVariantConst json)
{
  bool changed = false;
#define APPLY_VALUE(KEY, TARGET) if (!json[KEY].isNull()) { TARGET = json[KEY]; changed = true; }
  APPLY_VALUE("Enabled", commandedValues.Heating.Active)
  APPLY_VALUE("FeedSetpoint", commandedValues.Heating.FeedSetpoint)
  APPLY_VALUE("FeedBaseSetpoint", commandedValues.Heating.BasepointTemperature)
  APPLY_VALUE("FeedCutOff", commandedValues.Heating.EndpointTemperature)
  APPLY_VALUE("FeedMinimum", commandedValues.Heating.MinimumFeedTemperature)
  APPLY_VALUE("AuxiliaryTemperature", commandedValues.Heating.AuxiliaryTemperature)
  APPLY_VALUE("AmbientTemperature", commandedValues.Heating.AmbientTemperature)
  APPLY_VALUE("TargetAmbientTemperature", commandedValues.Heating.TargetAmbientTemperature)
  APPLY_VALUE("Adaption", commandedValues.Heating.FeedAdaption)
  APPLY_VALUE("ValveScaling", commandedValues.Heating.ValveScaling)
  APPLY_VALUE("ValveScalingMaxOpening", commandedValues.Heating.MaxValveOpening)
  APPLY_VALUE("ValveScalingOpening", commandedValues.Heating.ValveOpening)
  APPLY_VALUE("DynamicAdaption", commandedValues.Heating.DynamicAdaption)
  APPLY_VALUE("OverrideSetpoint", commandedValues.Heating.OverrideSetpoint)
  APPLY_VALUE("OnDemandBoostDuration", commandedValues.Heating.BoostDuration)
#undef APPLY_VALUE
  if (changed)
  {
    commandedValues.Heating.FeedSetpoint = constrain(commandedValues.Heating.FeedSetpoint, 0.0, 100.0);
    commandedValues.Heating.MinimumFeedTemperature = constrain(commandedValues.Heating.MinimumFeedTemperature, 0.0, 100.0);
    commandedValues.Heating.MaxValveOpening = constrain(commandedValues.Heating.MaxValveOpening, 0, 100);
    commandedValues.Heating.ValveOpening = constrain(commandedValues.Heating.ValveOpening, 0, 100);
    commandedValues.Heating.BoostDuration = constrain(commandedValues.Heating.BoostDuration, 0, 86400);
    NotifyValidHeatingCommand();
    SetFeedTemperature();
  }
  return changed;
}

bool ApplyHotWaterCommand(JsonVariantConst json)
{
  if (json["Setpoint"].isNull())
    return false;
  commandedValues.HotWater.SetPoint = constrain(json["Setpoint"].as<int>(), 0, 100);
  return true;
}

void ApplyBoostCommand(bool enabled)
{
  commandedValues.Heating.Boost = enabled;
  commandedValues.Heating.BoostTimeCountdown = enabled ? commandedValues.Heating.BoostDuration : 0;
  NotifyValidHeatingCommand();
  SetFeedTemperature();
}

void ApplyFastHeatupCommand(bool enabled)
{
  commandedValues.Heating.FastHeatup = enabled;
  commandedValues.Heating.ReferenceAmbientTemperature = commandedValues.Heating.AmbientTemperature;
  NotifyValidHeatingCommand();
  SetFeedTemperature();
}

// \brief (Re)connect to MQTT broker
void reconnectMqtt()
{
  if (!WiFi.isConnected() || client.connected())
    return;

  const unsigned long now = millis();
  if (lastMqttAttempt != 0 && now - lastMqttAttempt < mqttRetryInterval)
    return;
  lastMqttAttempt = now;

  Log.print("Attempting MQTT connection...");

  const String clientId = generateClientId();
  bool connected = false;
  if (configuration.HomeAssistant.Enabled)
  {
    const String availability = configuration.HomeAssistant.StateTopic + "availability";
    connected = client.connect(clientId.c_str(),
                               configuration.Mqtt.User,
                               configuration.Mqtt.Password,
                               availability.c_str(),
                               0,
                               true,
                               "offline");
  }
  else
  {
    connected = client.connect(clientId.c_str(), configuration.Mqtt.User, configuration.Mqtt.Password);
  }

  if (!connected)
  {
    Log.printf("failed, rc=%d. Retrying in %lu seconds while CAN control continues.\r\n",
               client.state(), mqttRetryInterval / 1000);
    return;
  }

  Log.println("connected");
  client.subscribe(configuration.Mqtt.Topics.HeatingParameters);
  client.subscribe(configuration.Mqtt.Topics.WaterParameters);
  client.subscribe(configuration.Mqtt.Topics.StatusRequest);
  client.subscribe(configuration.Mqtt.Topics.Boost);
  client.subscribe(configuration.Mqtt.Topics.FastHeatup);
  if (configuration.HomeAssistant.Enabled)
  {
    SetupHomeAssistantDiscovery();
  }
}

// Returns a client id for MQTT communication
String generateClientId()
{
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  // Create client ID using MAC address
  String clientId = "ESP-";
  clientId += macAddress;

  return clientId;
}

void setupMqttClient()
{
  // Setup MQTT client
  client.setServer(configuration.Mqtt.Server, configuration.Mqtt.Port);
  client.setCallback(callback);
  client.setKeepAlive(10);
  // Bound the remaining synchronous part of PubSubClient connection attempts.
  // The ESP TCP connect and MQTT CONNACK waits must not stall boiler control.
  espClient.setConnectionTimeout(250);
  client.setSocketTimeout(1);
  if (!client.setBufferSize(16384))
    Log.println("Unable to allocate the MQTT buffer required for Home Assistant discovery.");
}

String boolToString(bool src)
{
  return (src) ? "true" : "false";
}

// Callback for MQTT subscribed topics
void callback(char *topic, byte *payload, unsigned int length)
{
  ShowActivityLed();
  String payloadBuf;
  payloadBuf.reserve(length);
  for (unsigned int i = 0; i < length; i++)
    payloadBuf += static_cast<char>(payload[i]);
  if (HandleHomeAssistantMessage(topic, payloadBuf))
    return;

  // Status Requested
  if (strcmp(topic, configuration.Mqtt.Topics.StatusRequest) == 0)
  {
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, (char *)payload, length);

    if (error)
    {
      Log.printf("[Status Request] Error Processing JSON: %s\r\n", error.c_str());
      return;
    }
    /* Example JSON:
        {
            "HeatingTemperatures": true,
            "WaterTemperatures": true,
            "AuxiliaryTemperatures": true,
            "Status": true
        }
    */

    bool HeatingTemperatures = false;
    bool WaterTemperatures = false;
    bool AuxiliaryTemperatures = false;
    bool Status = false;

    if (!doc["HeatingTemperatures"].isNull())
      HeatingTemperatures = doc["HeatingTemperatures"]; // false
    if (!doc["WaterTemperatures"].isNull())
      WaterTemperatures = doc["WaterTemperatures"]; // false
    if (!doc["AuxiliaryTemperatures"].isNull())
      AuxiliaryTemperatures = doc["AuxiliaryTemperatures"]; // true
    if (!doc["Status"].isNull())
      Status = doc["Status"]; // false

    if (HeatingTemperatures)
    {
      PublishHeatingTemperaturesAndStatus();
    }

    if (WaterTemperatures)
    {
      PublishWaterTemperatures();
    }

    if (AuxiliaryTemperatures)
    {
      PublishAuxiliaryTemperatures();
    }

    if (Status)
    {
      PublishStatus();
    }
  }

  // Receiving Heating Parameters
  if (strcmp(topic, configuration.Mqtt.Topics.HeatingParameters) == 0)
  {
    /*
    Example Json:
    {
      "Enabled": false,
      "FeedSetpoint": 0,
      "FeedBaseSetpoint": -10,
      "FeedCutOff": 22,
      "FeedMinimum": 10,
      "AuxiliaryTemperature": 11.6,
      "AmbientTemperature": 0,
      "TargetAmbientTemperature": 21,
      "OnDemandBoost": false,
      "OnDemandBoostDuration": 600,
      "FastHeatup": false,
      "Adaption": 0,
      "ValveScaling": true,
      "ValveScalingMaxOpening": 100,
      "ValveScalingOpening": 75,
      "DynamicAdaption": true,
      "OverrideSetpoint": false
    }
    */

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (char *)payload, length);

    if (error)
    {
      Log.printf("[Heating Parameters] Error Processing JSON: %s\r\n", error.c_str());
      return;
    }

    ApplyHeatingCommand(doc.as<JsonVariantConst>());

  }
  // Receiving Water Parameters
  if (strcmp(topic, configuration.Mqtt.Topics.WaterParameters) == 0)
  {

      /*
      Example Json:
      {
        "Setpoint": 40
      }
      */

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (char *)payload, length);

      if (error)
      {
        Log.printf("[Water Parameters] Error Processing JSON: %s\r\n", error.c_str());
        return;
      }

      ApplyHotWaterCommand(doc.as<JsonVariantConst>());
  }

  // On-Demand Boost
  if (strcmp(topic, configuration.Mqtt.Topics.Boost) == 0)
  {
    if (payloadBuf != "0" && payloadBuf != "1")
      return;
    int i = payloadBuf.toInt();
    ApplyBoostCommand(i == 1);
  }

  // Fast Heatup
  if (strcmp(topic, configuration.Mqtt.Topics.FastHeatup) == 0)
  {
    if (payloadBuf != "0" && payloadBuf != "1")
      return;
    int i = payloadBuf.toInt();
    ApplyFastHeatupCommand(i == 1);
  }
}

void PublishStatus()
{
  ShowActivityLed();
  /* Example JSON
  {
      "GasBurner": true,
      "Error": 0..255,
  }
  */
  JsonDocument doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc["General"].to<JsonObject>();
  }

  jsonObj["GasBurner"] = boolToString(ceraValues.General.FlameLit);
  jsonObj["Error"] = ceraValues.General.Error;
  jsonObj["FreeHeap"] = ESP.getFreeHeap();
  jsonObj["HeapSize"] = ESP.getHeapSize();
  jsonObj["FilesystemUsed"] = LittleFS.usedBytes();
  jsonObj["FilesystemSize"] = LittleFS.totalBytes();
  jsonObj["FlashSize"] = ESP.getFlashChipSize();
  jsonObj["ChipModel"] = ESP.getChipModel();
  const uint16_t chipRevision = ESP.getChipRevision();
  char chipRevisionText[8];
  snprintf(chipRevisionText, sizeof(chipRevisionText), "%u.%u", chipRevision / 100, chipRevision % 100);
  jsonObj["ChipRevision"] = chipRevisionText;
  jsonObj["CpuCores"] = ESP.getChipCores();
  jsonObj["CpuFrequency"] = ESP.getCpuFreqMHz();

  // Mute Flag Set. Don't send message.
  if (MUTE_MQTT == 1)
    return;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);

  // Send to HA state topic or the configured topic, when HA is disabled.
  if (configuration.HomeAssistant.Enabled)
  {
    String topic = configuration.HomeAssistant.StateTopic + "General/state";
    client.publish(topic.c_str(), reinterpret_cast<const uint8_t *>(buffer), n, true);
  }
  else
  {
    client.publish(configuration.Mqtt.Topics.Status, buffer, n);
  }
}

void PublishHeatingTemperaturesAndStatus()
{
  ShowActivityLed();
  /* Example JSON
  {
      "FeedMaximum": 75.10,
      "FeedCurrent": 30.10,
      "FeedSetpoint": 10.10,
      "Outside": 15.10,
      "Season": true,
      "Working": true,
      "Boost": true,
      "BoostTimeLeft": 600,
      "FastHeatup": true
  }
  */

  JsonDocument doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc["Heating"].to<JsonObject>();
  }

  jsonObj["FeedMaximum"] = ceraValues.Heating.FeedMaximum;
  jsonObj["FeedCurrent"] = ceraValues.Heating.FeedCurrent;
  jsonObj["FeedSetpoint"] = (OverrideControl) ? commandedValues.Heating.CalculatedFeedSetpoint : ceraValues.Heating.FeedSetpoint;
  jsonObj["Outside"] = ceraValues.General.OutsideTemperature;
  jsonObj["Pump"] = boolToString(ceraValues.Heating.PumpActive);
  jsonObj["Season"] = boolToString(ceraValues.Heating.Season);
  jsonObj["Working"] = boolToString(ceraValues.Heating.Active);
  jsonObj["Boost"] = boolToString(commandedValues.Heating.Boost);
  jsonObj["BoostTimeLeft"] = commandedValues.Heating.BoostTimeCountdown;
  jsonObj["FastHeatup"] = boolToString(commandedValues.Heating.FastHeatup);
  jsonObj["Enabled"] = boolToString(commandedValues.Heating.Active);
  jsonObj["RequestedFeedSetpoint"] = commandedValues.Heating.FeedSetpoint;
  jsonObj["BoostDuration"] = commandedValues.Heating.BoostDuration;
  jsonObj["RoomReferenceT"] = commandedValues.Heating.AmbientTemperature;

  // Mute Flag Set. Don't send message.
  if (MUTE_MQTT == 1)
    return;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);

  // Send to HA state topic or the configured topic, when HA is disabled.
  if (configuration.HomeAssistant.Enabled)
  {
    String topic = configuration.HomeAssistant.StateTopic + "Heating/state";
    client.publish(topic.c_str(), reinterpret_cast<const uint8_t *>(buffer), n, true);
  }
  else
  {
    client.publish(configuration.Mqtt.Topics.HeatingValues, buffer, n);
  }
}

void PublishWaterTemperatures()
{
  ShowActivityLed();
  // TODO: Gather HW temperatures
  /* Example JSON
    {
      "Maximum": 75.10,
      "Current": 30.10,
      "Setpoint": 10.10,
      "CFSetpoint": 20.00,
      "Now": true,
      "Buffer": false
    }
  */

  JsonDocument doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc["Water"].to<JsonObject>();
  }

  jsonObj["Maximum"] = ceraValues.Hotwater.MaximumTemperature;
  jsonObj["Current"] = ceraValues.Hotwater.TemperatureCurrent;
  jsonObj["Setpoint"] = ceraValues.Hotwater.SetPoint;
  jsonObj["CFSetpoint"] = ceraValues.Hotwater.ContinousFlowSetpoint;
  jsonObj["Now"] = boolToString(ceraValues.Hotwater.Now);
  jsonObj["Buffer"] = boolToString(ceraValues.Hotwater.BufferMode);

  // Mute Flag Set. Don't send message.
  if (MUTE_MQTT == 1)
    return;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);

  // Send to HA state topic or the configured topic, when HA is disabled.
  if (configuration.HomeAssistant.Enabled)
  {
    String topic = configuration.HomeAssistant.StateTopic + "Water/state";
    client.publish(topic.c_str(), reinterpret_cast<const uint8_t *>(buffer), n, true);
  }
  else
  {

    client.publish(configuration.Mqtt.Topics.WaterValues, buffer, n);
  }
}

void PublishAuxiliaryTemperatures()
{
  ShowActivityLed();
  /*
  {
      "Feed": 30.10,
      "Return": 30.10,
      "Exhaust": 50.10,
      "Ambient": 17.10
  }
  */

  JsonDocument doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc["Auxiliary"].to<JsonObject>();
  }

  for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
  {
    Sensor curSensor = configuration.TemperatureSensors.Sensors[i];
    JsonObject sensorVal = jsonObj[curSensor.Label].to<JsonObject>();
    sensorVal["Temperature"] = ceraValues.Auxiliary.Temperatures[i];
    sensorVal["Reachable"] = boolToString(curSensor.reachable);
  }

  // Mute Flag Set. Don't send message.
  if (MUTE_MQTT == 1)
    return;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);

  // Send to HA state topic or the configured topic, when HA is disabled.
  if (configuration.HomeAssistant.Enabled)
  {
    String topic = configuration.HomeAssistant.StateTopic + "Auxiliary/state";
    client.publish(topic.c_str(), reinterpret_cast<const uint8_t *>(buffer), n, true);
  }
  else
  {
    client.publish(configuration.Mqtt.Topics.AuxiliaryValues, buffer, n);
  }
}

void PublishLog(const char *msg, const char *func, LogLevel level)
{
  const size_t size = 1024;
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["lvl"] = level;
  root["fnc"] = func;
  root["msg"] = msg;
  char buf[size];

  size_t n = serializeJson(doc, buf);

  client.publish("cerasmarter/log", buf, n);
}

void ShowActivityLed()
{
  if (MqttActivityHandle == NULL)
  {
    xTaskCreate(ShowMqttActivity, "MQTT Activity", 2000, NULL, 1, &MqttActivityHandle);
  }
}
