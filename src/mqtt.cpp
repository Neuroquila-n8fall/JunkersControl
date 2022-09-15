#include <Arduino.h>
#include <mqtt.h>
#include <configuration.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>
#include <ArduinoJson.h>

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
    if (client.connect(clientId.c_str(), configuration.MQTT_User, configuration.MQTT_Password))
    {
      Serial.println("connected");

      // Subscribe to parameters.
      client.subscribe(configuration.MQTT_Topics_HeatingParameters);
      client.subscribe(configuration.MQTT_Topics_WaterParameters);
      client.subscribe(configuration.MQTT_Topics_Status);
      client.subscribe(configuration.MQTT_Topics_AuxilaryParameters);
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
  client.setServer(configuration.MQTT_Server, configuration.MQTT_Port);
  client.setCallback(callback);
  client.setKeepAlive(10);
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
  if (strcmp(topic, configuration.MQTT_Topics_Status) == 0)
  {
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, (char *)payload, length);

    if (error)
    {
      WriteToConsoles("[Status Request] Error Processing JSON: ");
      WriteToConsoles(error.c_str());
      WriteToConsoles("\r\n");
      return;
    }

    bool HeatingTemperatures = doc["HeatingTemperatures"];   // false
    bool WaterTemperatures = doc["WaterTemperatures"];       // false
    bool AuxilaryTemperatures = doc["AuxilaryTemperatures"]; // true
    bool Status = doc["Status"];                             // false

    if (HeatingTemperatures) PublishHeatingTemperatures();    

    if (WaterTemperatures) PublishWaterTemperatures();

    if (AuxilaryTemperatures) PublishAuxilaryTemperatures();

    if (Status) PublishStatus();

  }

  // Receiving Heating Parameters
  if (strcmp(topic, configuration.MQTT_Topics_HeatingParameters) == 0)
  {
    const int docSize = 384;
    StaticJsonDocument<docSize> doc;
    bool setFeedImmediately = false;
    DeserializationError error = deserializeJson(doc, (char *)payload, length);

    if (error)
    {
      WriteToConsoles("[Heating Parameters] Error Processing JSON: ");
      WriteToConsoles(error.c_str());
      WriteToConsoles("\r\n");
      return;
    }

    JsonObject Cerasmarter = doc["Cerasmarter"];
    //Request Enable/Disable Heating and set the status of the heating accordlingly
    commandedValues.Heating.Active = Cerasmarter["Enabled"];
    ceraValues.Heating.Active = commandedValues.Heating.Active;

    commandedValues.Heating.FeedSetpoint = Cerasmarter["FeedSetpoint"];
    commandedValues.Heating.BasepointTemperature = Cerasmarter["FeedBaseSetpoint"];
    commandedValues.Heating.EndpointTemperature = Cerasmarter["FeedCutOff"];    
    commandedValues.Heating.MinimumFeedTemperature = Cerasmarter["FeedMinimum"];    
    commandedValues.Heating.AuxilaryTemperature = Cerasmarter["AuxilaryTemperature"];
    commandedValues.Heating.AmbientTemperature = Cerasmarter["AmbientTemperature"];
    commandedValues.Heating.TargetAmbientTemperature = Cerasmarter["TargetAmbientTemperature"];

    
    if (Cerasmarter["OnDemandBoost"] != commandedValues.Heating.Boost)
    {
      commandedValues.Heating.Boost = Cerasmarter["OnDemandBoost"];
      commandedValues.Heating.BoostTimeCountdown = commandedValues.Heating.BoostDuration;
      setFeedImmediately = true;
    }

    commandedValues.Heating.BoostDuration = Cerasmarter["OnDemandBoostDuration"];
    if (Cerasmarter["FastHeatup"] != commandedValues.Heating.FastHeatup)
    {
      setFeedImmediately = true;
      // Set reference Temperature
      commandedValues.Heating.ReferenceAmbientTemperature = commandedValues.Heating.AmbientTemperature;
    }

    commandedValues.Heating.FastHeatup = Cerasmarter["FastHeatup"];    
    commandedValues.Heating.FeedAdaption = Cerasmarter["Adaption"];    
    commandedValues.Heating.ValveScaling = Cerasmarter["ValveScaling"];    
    commandedValues.Heating.MaxValveOpening = Cerasmarter["ValveScalingMaxOpening"];    
    commandedValues.Heating.ValveOpening = Cerasmarter["ValveScalingOpening"];    
    commandedValues.Heating.DynamicAdaption = Cerasmarter["DynamicAdaption"];    
    commandedValues.Heating.OverrideSetpoint = Cerasmarter["OverrideSetpoint"];

    if (setFeedImmediately)
      SetFeedTemperature();

    // Receiving Water Parameters
    if (strcmp(topic, configuration.MQTT_Topics_WaterParameters) == 0)
    {
      const int docSize = 16;
      StaticJsonDocument<docSize> doc;
      DeserializationError error = deserializeJson(doc, (char *)payload, length);

      if (error)
      {
        WriteToConsoles("[Water Parameters] Error Processing JSON: ");
        WriteToConsoles(error.c_str());
        WriteToConsoles("\r\n");
        return;
      }

      // TODO: Water Temperature Setpoint variable to be populated here...
      float Setpoint = doc["Setpoint"]; // 22.1
    }
  }
}

void PublishStatus()
{
  /* Example JSON
  {
		"Status":
		{
			"GasBurner": true,
			"Pump": true,
			"Error": 0..255,
			"Season": true,
			"Working": true,
			"Boost": true,
			"FastHeatup": true			
		}
  }
  */
  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();
  jsonObj["GasBurner"] = ceraValues.General.FlameLit;
  jsonObj["Pump"] = ceraValues.Heating.PumpActive;
  jsonObj["Error"] = ceraValues.General.Error;
  jsonObj["Season"] = ceraValues.Heating.Season;
  jsonObj["Working"] = ceraValues.Heating.Active;
  jsonObj["Boost"] = commandedValues.Heating.Boost;
  jsonObj["FastHeatup"] = commandedValues.Heating.FastHeatup;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);
  client.publish(StatusTopic, buffer, n);
}

void PublishHeatingTemperatures()
{
  /* Example JSON
  {
    "Temperatures":
		{
			"FeedMaximum": 75.10,
			"FeedCurrent": 30.10,
			"FeedSetpoint": 10.10,
			"Outside": 15.10
		}
  }
  */

  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();
  jsonObj["FeedMaximum"] = ceraValues.Heating.FeedMaximum;
  jsonObj["FeedCurrent"] = ceraValues.Heating.FeedCurrent;
  jsonObj["FeedSetpoint"] = commandedValues.Heating.CalculatedFeedSetpoint;
  jsonObj["Outside"] = ceraValues.General.OutsideTemperature;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);
  client.publish(HeatingTemperaturesTopic, buffer, n);
}

void PublishWaterTemperatures()
{
  //TODO: Gather HW temperatures
  /* Example JSON
  "Temperatures":
		{
			"FeedMaximum": 75.10,
			"FeedCurrent": 30.10,
			"FeedSetpoint": 10.10,
		}
  */

  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();
  jsonObj["FeedMaximum"] = 0;
  jsonObj["FeedCurrent"] = 0;
  jsonObj["FeedSetpoint"] = 0;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);
  client.publish(WaterTemperaturesTopic, buffer, n);
}

void PublishAuxilaryTemperatures()
{
  /*
  {
  "AuxilaryTemperatures":
		{
			"Feed": 30.10,
			"Return": 30.10,
			"Exhaust": 50.10,
			"Ambient": 17.10
		}
  }
  */

  StaticJsonDocument<384> doc;
  JsonObject jsonObj = doc.to<JsonObject>();
  jsonObj["Feed"] = ceraValues.Auxilary.FeedTemperature;
  jsonObj["Return"] = ceraValues.Auxilary.ReturnTemperature;
  jsonObj["Exhaust"] = ceraValues.Auxilary.ExhaustTemperature;
  jsonObj["Ambient"] = ceraValues.Auxilary.AmbientTemperature;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);
  client.publish(AuxilaryTemperaturesTopic, buffer, n);
}