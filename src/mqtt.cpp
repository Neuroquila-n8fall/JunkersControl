#include <Arduino.h>
#include <mqtt.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>
#include <ArduinoJson.h>

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Client (uses Wifi Client)
//——————————————————————————————————————————————————————————————————————————————
PubSubClient client(espClient);

//-- MQTT Endpoint
const char *mqttServer = mqttSERVER;
const char *mqttUsername = mqttUSERNAME;
const char *mqttPassword = mqttPASSWORD;

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Topics
//——————————————————————————————————————————————————————————————————————————————

//-- Subscriptions

const char subscription_Parameters[] = "heating/set/parameters";

const char pub_Parameters[] = "heating/get/parameters";

// Example Topic
const char subTopic_Example[] = "heizung/control/something";

// Heating ON|OFF Control
const char subscription_OnOff[] = "heizung/control/operation";

// Heating Setpoint Feed
const char subscription_FeedSetpoint[] = "heizung/control/sollvorlauf";

// Heating Adaption: The Outside temperature at which hcMaxFeed should be reached
const char subscription_FeedBaseSetpoint[] = "heizung/parameters/fusspunkt";

// Heating Adaption: The Outside temperature at which the minimum temperature should be set
const char subscription_FeedCutOff[] = "heizung/parameters/endpunkt";

// Heating Adaption: Minimum Temperature of Feed (Anti-Freeze)
const char subscription_FeedMinimum[] = "heizung/parameters/minimum";

// Secondary Outside Temperature Source
const char subscription_AuxTemperature[] = "umwelt/temperaturen/aussen";

// Ambient Temperature Source
const char subscription_AmbientTemperature[] = "heizung/parameters/ambient";

// Ambient Temperature Target
const char subscription_TargetAmbientTemperature[] = "heizung/parameters/targetambient";

// On-Demand Boost.
const char subscription_OnDemandBoost[] = "heizung/control/boost";

// On-Demand Boost: Time in seconds
const char subscription_OnDemandBoostDuration[] = "heizung/parameters/boostduration";

// Fast-Heatup
const char subscription_FastHeatup[] = "heizung/control/fastheatup";

// Adaption
const char subscription_Adaption[] = "heizung/parameters/adaption";

// Dynamic Adaption based on return temperature sensor and desired target room temperature
//   This enables a mode in which the feed temperature will be decreased when the return temperature is much higher than required
//   This is useful when the heating is much more capable than required.
//   Example: The return Temperature is 40°, the desired room temperature is 21°: The Adaption is -19° --> Feed setpoint is decreased by 19°
//   Another one: The Return temperature is 20°, the desired room temperature is 21°: The Adaption is +1° --> Feed setpoint increased by 1°
const char subscription_DynamicAdaption[] = "heizung/parameters/dynadapt";

const char subscription_OverrideSetpoint[] = "heizung/parameters/overridesetpoint";

//-- Published Topics

// Maximum Feed Temperature
const char pub_HcMaxFeedTemperature[] = "heizung/temperaturen/maxvorlauf";

// Current Feed Temperature
const char pub_CurFeedTemperature[] = "heizung/temperaturen/aktvorlauf";

// Setpoint of feed Temperature
const char pub_SetpointFeedTemperature[] = "heizung/temperaturen/sollvorlauf";

// The Temperature on the Outside
const char pub_OutsideTemperature[] = "heizung/temperaturen/aussen";

// Flame status
const char pub_GasBurner[] = "heizung/status/brenner";

// Heating Circuit Pump Status
const char pub_HcPump[] = "heizung/status/pumpe";

// General Error Flag
const char pub_Error[] = "heizung/status/fehler";

// Seasonal Operation Flag
const char pub_Season[] = "heizung/status/betriebsmodus";

// Heating Operating
const char pub_HcOperation[] = "heizung/status/heizbetrieb";

// Boost
const char pub_Boost[] = "heizung/status/boost";

// Fast Heatup
const char pub_Fastheatup[] = "heizung/status/schnellaufheizung";

// Valve Scaling
const char subscription_ValveScaling[] = "heizung/parameters/valvescaling";

// Valve Scaling Max Opening
const char subscription_ValveScalingMaxOpening[] = "heizung/parameters/valvemax";

// Valve Scaling Current Top Opening Value
const char subscription_ValveScalingOpening[] = "heizung/parameters/valvecurrent";

//-- Topics for Temperature Sensor Readings

// Feed Temperature Topic
const char pub_SensorFeed[] = "heizung/temperaturen/vorlauf";

// Return Temperature Topic
const char pub_SensorReturn[] = "heizung/temperaturen/nachlauf";

// Exhaust Temperature Topic
const char pub_SensorExhaust[] = "heizung/temperaturen/abgas";

// Ambient Temperature Topic
const char pub_SensorAmbient[] = "heizung/temperaturen/umgebung";

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
// Stored Ambient Temperatur as reference. Reset everytime the Fastheatup flag is set
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
    if (client.connect(clientId.c_str(), mqttUsername, mqttPassword))
    {
      Serial.println("connected");

      // ... and resubscribe to topics
      client.subscribe(subscription_Parameters);
      client.subscribe(pub_Parameters);
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
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
  client.setKeepAlive(10);
}

// \brief Callback for MQTT subscribed topics
void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String s = String((char *)payload);
  if (!s)
  {
    return;
  }

  // Example for performing an action on topic receive.
  if (strcmp(topic, subTopic_Example) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    // Do something with 'i'
  }

  // Example for performing an action on topic receive.
  if (strcmp(topic, subscription_Parameters) == 0)
  {
    const int docSize = 768;
    StaticJsonDocument<docSize> doc;
    bool setFeedImmediately = false;
    DeserializationError error = deserializeJson(doc, (char *)payload, length);

    if (error)
    {
      WriteToConsoles("Error Processing JSON: ");
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
  }
}

void SendParameters()
{
  StaticJsonDocument<384> doc;
  JsonObject object = doc.to<JsonObject>();
  JsonObject HeatingInformation = object.createNestedObject("HeatingInformation");

  JsonObject HeatingInformation_Temperatures = HeatingInformation.createNestedObject("Temperatures");
  HeatingInformation_Temperatures["FeedMaximum"] = hcMaxFeed;
  HeatingInformation_Temperatures["FeedCurrent"] = hcCurrentFeed;
  HeatingInformation_Temperatures["FeedSetpoint"] = mqttCommandedFeedTemperature;
  HeatingInformation_Temperatures["Outside"] = OutsideTemperatureSensor;

  JsonObject HeatingInformation_AuxilaryTemperatures = HeatingInformation.createNestedObject("AuxilaryTemperatures");
  HeatingInformation_AuxilaryTemperatures["Feed"] = mqttAuxFeed;
  HeatingInformation_AuxilaryTemperatures["Return"] = mqttAuxReturn;
  HeatingInformation_AuxilaryTemperatures["Exhaust"] = mqttAuxExhaust;
  HeatingInformation_AuxilaryTemperatures["Ambient"] = mqttAuxAmbient;

  JsonObject HeatingInformation_Status = HeatingInformation.createNestedObject("Status");
  HeatingInformation_Status["GasBurner"] = flame;
  HeatingInformation_Status["Pump"] = hcPump;
  HeatingInformation_Status["Error"] = mqttErrorCode;
  HeatingInformation_Status["Season"] = hcSeason;
  HeatingInformation_Status["Working"] = hcActive;
  HeatingInformation_Status["Boost"] = mqttBoost;
  HeatingInformation_Status["FastHeatup"] = mqttFastHeatup;

  char buffer[768];
  size_t n = serializeJson(doc, buffer);
  client.publish(pub_Parameters, buffer, n);
}