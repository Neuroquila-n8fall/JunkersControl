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
    Serial.println("Can't connect to MQTT broker. [No Network]");
    return;
  }

  // Loop until we're reconnected
  while (!client.connected())
  {

    Serial.print("Attempting MQTT connection...");

    String clientId = generateClientId();
    // Attempt to connect
    if (client.connect(clientId.c_str(), configuration.Mqtt.User, configuration.Mqtt.Password))
    {
      Serial.println("connected");

      // Subscribe to parameters.
      client.subscribe(configuration.Mqtt.Topics.HeatingParameters);
      client.subscribe(configuration.Mqtt.Topics.WaterParameters);
      client.subscribe(configuration.Mqtt.Topics.StatusRequest);
      if (configuration.HomeAssistant.Enabled)
      {
        SetupAutodiscovery(HaSensorsFileName);
        SetupAutodiscovery(HaBinarySensorsFileName);
      }
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
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

String boolToJsonValue(bool src)
{
  return (src) ? "true" : "false";
}

// Callback for MQTT subscribed topics
void callback(char *topic, byte *payload, unsigned int length)
{
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
            "AuxilaryTemperatures": true,
            "Status": true
        }
    */
    bool HeatingTemperatures = doc["HeatingTemperatures"];   // false
    bool WaterTemperatures = doc["WaterTemperatures"];       // false
    bool AuxilaryTemperatures = doc["AuxilaryTemperatures"]; // true
    bool Status = doc["Status"];                             // false

    if (HeatingTemperatures)
    {
      PublishHeatingTemperatures();
    }

    if (WaterTemperatures)
    {
      PublishWaterTemperatures();
    }

    if (AuxilaryTemperatures)
    {
      PublishAuxilaryTemperatures();
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
      "AuxilaryTemperature": 11.6,
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
    bool setFeedImmediately = false;
    DeserializationError error = deserializeJson(doc, (char *)payload, length);

    if (error)
    {
      Log.printf("[Heating Parameters] Error Processing JSON: %s\r\n", error.c_str());
      return;
    }

    // Request Enable/Disable Heating and set the status of the heating accordingly
    commandedValues.Heating.Active = doc["Enabled"];

    commandedValues.Heating.FeedSetpoint = doc["FeedSetpoint"];
    commandedValues.Heating.BasepointTemperature = doc["FeedBaseSetpoint"];
    commandedValues.Heating.EndpointTemperature = doc["FeedCutOff"];
    commandedValues.Heating.MinimumFeedTemperature = doc["FeedMinimum"];
    commandedValues.Heating.AuxilaryTemperature = doc["AuxilaryTemperature"];
    commandedValues.Heating.AmbientTemperature = doc["AmbientTemperature"];
    commandedValues.Heating.TargetAmbientTemperature = doc["TargetAmbientTemperature"];

    if (doc["OnDemandBoost"] != commandedValues.Heating.Boost)
    {
      commandedValues.Heating.Boost = doc["OnDemandBoost"];
      commandedValues.Heating.BoostTimeCountdown = commandedValues.Heating.BoostDuration;
      setFeedImmediately = true;
    }

    commandedValues.Heating.BoostDuration = doc["OnDemandBoostDuration"];
    if (doc["FastHeatup"] != commandedValues.Heating.FastHeatup)
    {
      setFeedImmediately = true;
      // Set reference Temperature
      commandedValues.Heating.ReferenceAmbientTemperature = commandedValues.Heating.AmbientTemperature;
    }

    commandedValues.Heating.FastHeatup = doc["FastHeatup"];
    commandedValues.Heating.FeedAdaption = doc["Adaption"];
    commandedValues.Heating.ValveScaling = doc["ValveScaling"];
    commandedValues.Heating.MaxValveOpening = doc["ValveScalingMaxOpening"];
    commandedValues.Heating.ValveOpening = doc["ValveScalingOpening"];
    commandedValues.Heating.DynamicAdaption = doc["DynamicAdaption"];
    commandedValues.Heating.OverrideSetpoint = doc["OverrideSetpoint"];

    // Dispatch Feed Setpoint immediately
    if (setFeedImmediately)
      SetFeedTemperature();

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

      // TODO: Water Temperature Setpoint variable to be populated here...
      commandedValues.HotWater.SetPoint = doc["Setpoint"]; // 22.1
    }
  }
}

void PublishStatus()
{
  /* Example JSON
  {
      "GasBurner": true,
      "Pump": true,
      "Error": 0..255,
      "Season": true,
      "Working": true,
      "Boost": true,
      "FastHeatup": true
  }
  */
  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();

  // Create a parent block for HA
  if (configuration.HomeAssistant.Enabled)
  {
    jsonObj = doc.createNestedObject("General");
  }

  jsonObj["GasBurner"] = boolToJsonValue(ceraValues.General.FlameLit);
  jsonObj["Error"] = ceraValues.General.Error;

  if (Debug)
  {
    Log.println("//START\r\n[MQTT - SEND STATUS]");
    serializeJsonPretty(doc, Log);
    Log.println("//END");
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
    String topic = configuration.HomeAssistant.StateTopic + "General/state";
    client.publish(topic.c_str(), buffer, n);
  }
  else
  {
    client.publish(configuration.Mqtt.Topics.Status, buffer, n);
  }
}

void PublishHeatingTemperatures()
{
  /* Example JSON
  {
      "FeedMaximum": 75.10,
      "FeedCurrent": 30.10,
      "FeedSetpoint": 10.10,
      "Outside": 15.10
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
  jsonObj["FeedSetpoint"] = (Override) ? commandedValues.Heating.CalculatedFeedSetpoint : ceraValues.Heating.FeedSetpoint;
  jsonObj["Outside"] = ceraValues.General.OutsideTemperature;
  jsonObj["Pump"] = boolToJsonValue(ceraValues.Heating.PumpActive);
  jsonObj["Season"] = boolToJsonValue(ceraValues.Heating.Season);
  jsonObj["Working"] = boolToJsonValue(ceraValues.Heating.Active);
  jsonObj["Boost"] = boolToJsonValue(commandedValues.Heating.Boost);
  jsonObj["FastHeatup"] = boolToJsonValue(commandedValues.Heating.FastHeatup);

  if (Debug)
  {
    Log.println("//START\r\n[MQTT - SEND HEATING]");
    serializeJsonPretty(doc, Log);
    Log.println("//END");
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
  jsonObj["Current"] = ceraValues.Hotwater.TemperatureCurrent;
  jsonObj["Setpoint"] = ceraValues.Hotwater.SetPoint;
  jsonObj["CFSetpoint"] = ceraValues.Hotwater.ContinousFlowSetpoint;
  jsonObj["Now"] = boolToJsonValue(ceraValues.Hotwater.Now);
  jsonObj["Buffer"] = boolToJsonValue(ceraValues.Hotwater.BufferMode);

  if (Debug)
  {
    Log.println("//START\r\n[MQTT - SEND WATER]");
    serializeJsonPretty(doc, Log);
    Log.println("//END");
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
    String topic = configuration.HomeAssistant.StateTopic + "Water/state";
    client.publish(topic.c_str(), buffer, n);
  }
  else
  {

    client.publish(configuration.Mqtt.Topics.WaterValues, buffer, n);
  }
}

void PublishAuxilaryTemperatures()
{
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
    jsonObj = doc.createNestedObject("Auxilary");
  }

  for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
  {
    Sensor curSensor = configuration.TemperatureSensors.Sensors[i];
    jsonObj[curSensor.Label] = ceraValues.Auxilary.Temperatures[i];
  }

  if (Debug)
  {
    Log.println("//START\r\n[MQTT - SEND AUX]");
    serializeJsonPretty(doc, Log);
    Log.println("//END");
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
    String topic = configuration.HomeAssistant.StateTopic + "Auxilary/state";
    client.publish(topic.c_str(), buffer, n);
  }
  else
  {
    client.publish(configuration.Mqtt.Topics.AuxilaryValues, buffer, n);
  }
}
