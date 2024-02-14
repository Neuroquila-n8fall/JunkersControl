#include <Arduino.h>
#include <mqtt.h>
#include <configuration.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>
#include <ArduinoJson.h>
#include <ha_autodiscovery.h>

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Client (uses Wifi Client)
//——————————————————————————————————————————————————————————————————————————————
PubSubClient client(espClient);

CommandedValues commandedValues;

// \brief (Re)connect to MQTT broker
void reconnectMqtt()
{
  if (!WiFi.isConnected())
  {
    Log.println("Can't connect to MQTT broker. [No Network]");
    return;
  }

  // Loop until we're reconnected
  while (!client.connected())
  {

    Log.print("Attempting MQTT connection...");

    String clientId = generateClientId();
    // Attempt to connect
    if (client.connect(clientId.c_str(), configuration.Mqtt.User, configuration.Mqtt.Password))
    {
      Log.println("connected");

      // Subscribe to parameters.
      client.subscribe(configuration.Mqtt.Topics.HeatingParameters);
      client.subscribe(configuration.Mqtt.Topics.WaterParameters);
      client.subscribe(configuration.Mqtt.Topics.StatusRequest);
      client.subscribe(configuration.Mqtt.Topics.Boost);
      client.subscribe(configuration.Mqtt.Topics.FastHeatup);
      if (configuration.HomeAssistant.Enabled)
      {
        SetupAutodiscovery(HaSensorsFileName);
        SetupAutodiscovery(HaBinarySensorsFileName);
      }
    }
    else
    {
      Log.print("failed, rc=");
      Log.print(client.state());
      Log.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
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
}

String boolToString(bool src)
{
  return (src) ? "true" : "false";
}

// Callback for MQTT subscribed topics
void callback(char *topic, byte *payload, unsigned int length)
{
  ShowActivityLed();
  payload[length] = '\0';
  String s = String((char *)payload);
  if (!s)
  {
    return;
  }

  // Status Requested
  if (strcmp(topic, configuration.Mqtt.Topics.StatusRequest) == 0)
  {
    StaticJsonDocument<256> doc;

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

    const int docSize = 384;
    StaticJsonDocument<docSize> doc;
    DeserializationError error = deserializeJson(doc, (char *)payload, length);

    if (error)
    {
      Log.printf("[Heating Parameters] Error Processing JSON: %s\r\n", error.c_str());
      return;
    }

    // Request Enable/Disable Heating and set the status of the heating accordingly
    if (!doc["Enabled"].isNull())
      commandedValues.Heating.Active = doc["Enabled"];
    if (!doc["FeedSetpoint"].isNull())
      commandedValues.Heating.FeedSetpoint = doc["FeedSetpoint"];
    if (!doc["FeedBaseSetpoint"].isNull())
      commandedValues.Heating.BasepointTemperature = doc["FeedBaseSetpoint"];
    if (!doc["FeedCutOff"].isNull())
      commandedValues.Heating.EndpointTemperature = doc["FeedCutOff"];
    if (!doc["FeedMinimum"].isNull())
      commandedValues.Heating.MinimumFeedTemperature = doc["FeedMinimum"];
    if (!doc["AuxiliaryTemperature"].isNull())
      commandedValues.Heating.AuxiliaryTemperature = doc["AuxiliaryTemperature"];
    if (!doc["AmbientTemperature"].isNull())
      commandedValues.Heating.AmbientTemperature = doc["AmbientTemperature"];
    if (!doc["TargetAmbientTemperature"].isNull())
      commandedValues.Heating.TargetAmbientTemperature = doc["TargetAmbientTemperature"];
    if (!doc["Adaption"].isNull())
      commandedValues.Heating.FeedAdaption = doc["Adaption"];
    if (!doc["ValveScaling"].isNull())
      commandedValues.Heating.ValveScaling = doc["ValveScaling"];
    if (!doc["ValveScalingMaxOpening"].isNull())
      commandedValues.Heating.MaxValveOpening = doc["ValveScalingMaxOpening"];
    if (!doc["ValveScalingOpening"].isNull())
      commandedValues.Heating.ValveOpening = doc["ValveScalingOpening"];
    if (!doc["DynamicAdaption"].isNull())
      commandedValues.Heating.DynamicAdaption = doc["DynamicAdaption"];
    if (!doc["OverrideSetpoint"].isNull())
      commandedValues.Heating.OverrideSetpoint = doc["OverrideSetpoint"];
    if (!doc["OnDemandBoostDuration"].isNull())
      commandedValues.Heating.BoostDuration = doc["OnDemandBoostDuration"];

    // Receiving Water Parameters
    if (strcmp(topic, configuration.Mqtt.Topics.WaterParameters) == 0)
    {

      /*
      Example Json:
      {
        "Setpoint": 40
      }
      */

      const int docSize = 16;
      StaticJsonDocument<docSize> doc;
      DeserializationError error = deserializeJson(doc, (char *)payload, length);

      if (error)
      {
        Log.printf("[Water Parameters] Error Processing JSON: %s\r\n", error.c_str());
        return;
      }

      if (!doc["Setpoint"].isNull())
        commandedValues.HotWater.SetPoint = doc["Setpoint"]; // 22.1
    }
  }

  // On-Demand Boost
  if (strcmp(topic, configuration.Mqtt.Topics.Boost) == 0)
  {
    int i = s.toInt();
    commandedValues.Heating.Boost = i == 1;
    commandedValues.Heating.BoostTimeCountdown = commandedValues.Heating.BoostDuration;
    SetFeedTemperature();
  }

  // Fast Heatup
  if (strcmp(topic, configuration.Mqtt.Topics.FastHeatup) == 0)
  {
    int i = s.toInt();
    commandedValues.Heating.FastHeatup = i == 1;
    commandedValues.Heating.ReferenceAmbientTemperature = commandedValues.Heating.AmbientTemperature;
    SetFeedTemperature();
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
  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc.createNestedObject("General");
  }

  jsonObj["GasBurner"] = boolToString(ceraValues.General.FlameLit);
  jsonObj["Error"] = ceraValues.General.Error;

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
    client.publish(topic.c_str(), buffer, n);
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

  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc.createNestedObject("Heating");
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
    client.publish(topic.c_str(), buffer, n);
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

  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc.createNestedObject("Water");
  }

  jsonObj["Maximum"] = ceraValues.Hotwater.MaximumTemperature;
  jsonObj["Current"] = ceraValues.Hotwater.CurrentTemperature;
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
    client.publish(topic.c_str(), buffer, n);
  }
  else
  {

    client.publish(configuration.Mqtt.Topics.WaterValues, buffer, n);
  }
}

void PublishMixedCircuitTemperaturesAndStatus()
{
  ShowActivityLed();
  /* Example JSON
    {
      "PumpActive": true,
      "Economy": false,
      "FeedSetpoint": 10.10,
      "FeedCurrent": 20.00,
      "MixValveOpen": true,
    }
  */

  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc.createNestedObject("MixedCircuit");
  }

  jsonObj["PumpActive"] = boolToString(ceraValues.MixedCircuit.PumpActive);
  jsonObj["Economy"] = boolToString(ceraValues.MixedCircuit.Economy);
  jsonObj["FeedSetpoint"] = ceraValues.MixedCircuit.FeedSetpoint;
  jsonObj["FeedCurrent"] = ceraValues.MixedCircuit.FeedCurrent;
  jsonObj["MixValveOpen"] = ceraValues.MixedCircuit.MixValveOpen;
  
  // Mute Flag Set. Don't send message.
  if (MUTE_MQTT == 1)
    return;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);

  // Send to HA state topic or the configured topic, when HA is disabled.
  if (configuration.HomeAssistant.Enabled)
  {
    String topic = configuration.HomeAssistant.StateTopic + "MixedCircuit/state";
    client.publish(topic.c_str(), buffer, n);
  }
  else
  {

    client.publish(configuration.Mqtt.Topics.MixedCircuitValues, buffer, n);
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

  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc.createNestedObject("Auxiliary");
  }

  for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
  {
    Sensor curSensor = configuration.TemperatureSensors.Sensors[i];
    JsonObject sensorVal = jsonObj.createNestedObject(curSensor.Label);
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
    client.publish(topic.c_str(), buffer, n);
  }
  else
  {
    client.publish(configuration.Mqtt.Topics.AuxiliaryValues, buffer, n);
  }
}

void PublishLog(const char *msg, const char *func, LogLevel level)
{
  const size_t size = 1024;
  StaticJsonDocument<size> doc;
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
