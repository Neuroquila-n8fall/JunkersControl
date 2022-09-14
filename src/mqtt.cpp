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

//-- MQTT Endpoint
//const char *mqttServer = mqttSERVER;
//const char *mqttUsername = mqttUSERNAME;
//const char *mqttPassword = mqttPASSWORD;

//-- Variables set by MQTT subscriptions with factory defaults at startup

// Feed Temperature
double mqttFeedTemperatureSetpoint = 50.0F;
// Calculated Feed Temperature, Commanded
double mqttCommandedFeedTemperature = 50.0F;
// Basepoint Temperature
double mqttBasepointTemperature = -10.0F;
// Endpoint Temperature
double mqttEndpointTemperature = 31.0F;
// Ambient Temperature
double mqttAmbientTemperature = 17.0F;
// Target Ambient Temperature to reach
double mqttTargetAmbientTemperature = 21.5F;
// Feed Temperature Adaption Value. Increases or decreases the target feed temperature
double mqttFeedAdaption = 0.00F;
// Minimum ("Anti Freeze") Temperature.
double mqttMinimumFeedTemperature = 10.0F;
// Whether the heating should be off or not
bool mqttHeatingSwitch = true;
// Auxilary Temperature
double mqttAuxilaryTemperature = 0.0F;
// Boost will max the feed temperature for mqttBoostTime seconds
bool mqttBoost = false;
// Boost Duration (Seconds)
int mqttBoostDuration = 300;
// Countdown variable for boost control.
int boostTimeCountdown = mqttBoostDuration;
// Fast Heatup. This will max out the feed temperature for a prolongued time until mqttTargetAmbientTemperature has been reached. Setting this to false will return to normal mode in any case.
bool mqttFastHeatup = false;
// Stored Ambient Temperature as reference. Reset everytime the Fastheatup flag is set
double mqttReferenceAmbientTemperature = 17.0F;
// Dynamic Adaption Flag
bool mqttDynamicAdaption = false;
// Take the value from mqttFeedTemperatureSetpoint instead of doing built-in calculations.
bool mqttOverrideSetpoint = false;
// Enable scaling based upon valve opening
bool mqttValveScaling = false;
// Maximum valve opening. 80% for Homematic eTRV-2 (%)
int mqttMaxValveOpening = 80;
// The value of the valve that is most open (%)
int mqttValveOpening = 50;
// Error Code
int mqttErrorCode = 0x000;

// Auxilary Sensor Feed
double mqttAuxFeed = 0.0F;
// Auxilary Sensor Return
double mqttAuxReturn = 0.0F;
// Auxilary Sensor Exhaust
double mqttAuxExhaust = 0.0F;
// Auxilary Sensor Ambient
double mqttAuxAmbient = 0.0F;

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
    mqttHeatingSwitch = Cerasmarter["Enabled"];
    hcActive = mqttHeatingSwitch;
    mqttFeedTemperatureSetpoint = Cerasmarter["FeedSetpoint"];
    mqttBasepointTemperature = Cerasmarter["FeedBaseSetpoint"];
    mqttEndpointTemperature = Cerasmarter["FeedCutOff"];
    mqttMinimumFeedTemperature = Cerasmarter["FeedMinimum"];
    mqttAuxilaryTemperature = Cerasmarter["AuxilaryTemperature"];
    mqttAmbientTemperature = Cerasmarter["AmbientTemperature"];
    mqttTargetAmbientTemperature = Cerasmarter["TargetAmbientTemperature"];

    if (Cerasmarter["OnDemandBoost"] != mqttBoost)
    {
      mqttBoost = Cerasmarter["OnDemandBoost"];
      boostTimeCountdown = mqttBoostDuration;
      setFeedImmediately = true;
    }

    mqttBoostDuration = Cerasmarter["OnDemandBoostDuration"];
    if (Cerasmarter["FastHeatup"] != mqttFastHeatup)
    {
      setFeedImmediately = true;
      // Set reference Temperature
      mqttReferenceAmbientTemperature = mqttAmbientTemperature;
    }

    mqttFastHeatup = Cerasmarter["FastHeatup"];
    mqttFeedAdaption = Cerasmarter["Adaption"];
    mqttValveScaling = Cerasmarter["ValveScaling"];
    mqttMaxValveOpening = Cerasmarter["ValveScalingMaxOpening"];
    mqttValveOpening = Cerasmarter["ValveScalingOpening"];
    mqttDynamicAdaption = Cerasmarter["DynamicAdaption"];
    mqttOverrideSetpoint = Cerasmarter["OverrideSetpoint"];

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
  jsonObj["GasBurner"] = flame;
  jsonObj["Pump"] = hcPump;
  jsonObj["Error"] = mqttErrorCode;
  jsonObj["Season"] = hcSeason;
  jsonObj["Working"] = hcActive;
  jsonObj["Boost"] = mqttBoost;
  jsonObj["FastHeatup"] = mqttFastHeatup;

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
  jsonObj["FeedMaximum"] = hcMaxFeed;
  jsonObj["FeedCurrent"] = hcCurrentFeed;
  jsonObj["FeedSetpoint"] = mqttCommandedFeedTemperature;
  jsonObj["Outside"] = OutsideTemperatureSensor;

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
  jsonObj["Feed"] = mqttAuxFeed;
  jsonObj["Return"] = mqttAuxReturn;
  jsonObj["Exhaust"] = mqttAuxExhaust;
  jsonObj["Ambient"] = mqttAuxAmbient;

  // Publish Data on MQTT
  char buffer[768];
  size_t n = serializeJson(doc, buffer);
  client.publish(AuxilaryTemperaturesTopic, buffer, n);
}