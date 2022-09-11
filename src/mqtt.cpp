#include <Arduino.h>
#include <mqtt.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>

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

//Example Topic
const char subTopic_Example[] = "heizung/control/something";

//Heating ON|OFF Control
const char subscription_OnOff[] = "heizung/control/operation";

//Heating Setpoint Feed
const char subscription_FeedSetpoint[] = "heizung/control/sollvorlauf";

//Heating Adaption: The Outside temperature at which hcMaxFeed should be reached
const char subscription_FeedBaseSetpoint[] = "heizung/parameters/fusspunkt";

//Heating Adaption: The Outside temperature at which the minimum temperature should be set
const char subscription_FeedCutOff[] = "heizung/parameters/endpunkt";

//Heating Adaption: Minimum Temperature of Feed (Anti-Freeze)
const char subscription_FeedMinimum[] = "heizung/parameters/minimum";

//Secondary Outside Temperature Source
const char subscription_AuxTemperature[] = "umwelt/temperaturen/aussen";

//Ambient Temperature Source
const char subscription_AmbientTemperature[] = "heizung/parameters/ambient";

//Ambient Temperature Target
const char subscription_TargetAmbientTemperature[] = "heizung/parameters/targetambient";

//On-Demand Boost.
const char subscription_OnDemandBoost[] = "heizung/control/boost";

//On-Demand Boost: Time in seconds
const char subscription_OnDemandBoostDuration[] = "heizung/parameters/boostduration";

//Fast-Heatup
const char subscription_FastHeatup[] = "heizung/control/fastheatup";

//Adaption
const char subscription_Adaption[] = "heizung/parameters/adaption";

//Dynamic Adaption based on return temperature sensor and desired target room temperature
//  This enables a mode in which the feed temperature will be decreased when the return temperature is much higher than required
//  This is useful when the heating is much more capable than required.
//  Example: The return Temperature is 40°, the desired room temperature is 21°: The Adaption is -19° --> Feed setpoint is decreased by 19°
//  Another one: The Return temperature is 20°, the desired room temperature is 21°: The Adaption is +1° --> Feed setpoint increased by 1°
const char subscription_DynamicAdaption[] = "heizung/parameters/dynadapt";

const char subscription_OverrideSetpoint[] = "heizung/parameters/overridesetpoint";

//-- Published Topics

//Maximum Feed Temperature
const char pub_HcMaxFeedTemperature[] = "heizung/temperaturen/maxvorlauf";

//Current Feed Temperature
const char pub_CurFeedTemperature[] = "heizung/temperaturen/aktvorlauf";

//Setpoint of feed Temperature
const char pub_SetpointFeedTemperature[] = "heizung/temperaturen/sollvorlauf";

//The Temperature on the Outside
const char pub_OutsideTemperature[] = "heizung/temperaturen/aussen";

//Flame status
const char pub_GasBurner[] = "heizung/status/brenner";

//Heating Circuit Pump Status
const char pub_HcPump[] = "heizung/status/pumpe";

//General Error Flag
const char pub_Error[] = "heizung/status/fehler";

//Seasonal Operation Flag
const char pub_Season[] = "heizung/status/betriebsmodus";

//Heating Operating
const char pub_HcOperation[] = "heizung/status/heizbetrieb";

//Boost
const char pub_Boost[] = "heizung/status/boost";

//Fast Heatup
const char pub_Fastheatup[] = "heizung/status/schnellaufheizung";

//Valve Scaling
const char subscription_ValveScaling[] = "heizung/parameters/valvescaling";

//Valve Scaling Max Opening
const char subscription_ValveScalingMaxOpening[] = "heizung/parameters/valvemax";

//Valve Scaling Current Top Opening Value
const char subscription_ValveScalingOpening[] = "heizung/parameters/valvecurrent";

//-- Topics for Temperature Sensor Readings

//Feed Temperature Topic
const char pub_SensorFeed[] = "heizung/temperaturen/vorlauf";

//Return Temperature Topic
const char pub_SensorReturn[] = "heizung/temperaturen/nachlauf";

//Exhaust Temperature Topic
const char pub_SensorExhaust[] = "heizung/temperaturen/abgas";

//Ambient Temperature Topic
const char pub_SensorAmbient[] = "heizung/temperaturen/umgebung";

//-- Variables set by MQTT subscriptions with factory defaults at startup

//Feed Temperature
double mqttFeedTemperatureSetpoint = 50.0F;
//Basepoint Temperature
double mqttBasepointTemperature = -10.0F;
//Endpoint Temperature
double mqttEndpointTemperature = 31.0F;
//Ambient Temperature
double mqttAmbientTemperature = 17.0F;
//Target Ambient Temperature to reach
double mqttTargetAmbientTemperature = 21.5F;
//Feed Temperature Adaption Value. Increases or decreases the target feed temperature
double mqttFeedAdaption = 0.00F;
//Minimum ("Anti Freeze") Temperature.
double mqttMinimumFeedTemperature = 10.0F;
//Whether the heating should be off or not
bool mqttHeatingSwitch = true;
//Auxilary Temperature
double mqttAuxilaryTemperature = 0.0F;
//Boost will max the feed temperature for mqttBoostTime seconds
bool mqttBoost = false;
//Boost Duration (Seconds)
int mqttBoostDuration = 300;
//Countdown variable for boost control.
int boostTimeCountdown = mqttBoostDuration;
//Fast Heatup. This will max out the feed temperature for a prolongued time until mqttTargetAmbientTemperature has been reached. Setting this to false will return to normal mode in any case.
bool mqttFastHeatup = false;
//Stored Ambient Temperatur as reference. Reset everytime the Fastheatup flag is set
double mqttReferenceAmbientTemperature = 17.0F;
//Dynamic Adaption Flag
bool mqttDynamicAdaption = false;
//Take the value from mqttFeedTemperatureSetpoint instead of doing built-in calculations.
bool mqttOverrideSetpoint = false;
//Enable scaling based upon valve opening
bool mqttValveScaling = false;
//Maximum valve opening. 80% for Homematic eTRV-2 (%)
int mqttMaxValveOpening = 80;
//The value of the valve that is most open (%)
int mqttValveOpening = 50;

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
      client.subscribe(subTopic_Example);
      client.subscribe(subscription_AuxTemperature);
      client.subscribe(subscription_AmbientTemperature);
      client.subscribe(subscription_TargetAmbientTemperature);
      client.subscribe(subscription_Adaption);
      client.subscribe(subscription_FeedBaseSetpoint);
      client.subscribe(subscription_FeedCutOff);
      client.subscribe(subscription_FeedSetpoint);
      client.subscribe(subscription_FeedMinimum);
      client.subscribe(subscription_OnOff);
      client.subscribe(subscription_OnDemandBoost);
      client.subscribe(subscription_OnDemandBoostDuration);
      client.subscribe(subscription_FastHeatup);
      client.subscribe(subscription_DynamicAdaption);
      client.subscribe(subscription_OverrideSetpoint);
      client.subscribe(subscription_ValveScaling);
      client.subscribe(subscription_ValveScalingMaxOpening);
      client.subscribe(subscription_ValveScalingOpening);
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

//Returns a client id for MQTT communication
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
  //Setup MQTT client
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

  // Read Auxilary Outside Temperature
  if (strcmp(topic, subscription_AuxTemperature) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: AuxTemp >> " + s + "\r\n");
    }
    mqttAuxilaryTemperature = d;
  }

  //Read Heating Basepoint
  if (strcmp(topic, subscription_FeedBaseSetpoint) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: BasePoint >> " + s + "\r\n");
    }
    mqttBasepointTemperature = d;
  }

  //Read Heating Endpoint (Cut-Off)
  if (strcmp(topic, subscription_FeedCutOff) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Endpoint (Cut-Off) >> " + s + "\r\n");
    }
    mqttEndpointTemperature = d;
  }

  //Read Heating Minimum Temperature (Anti-Freeze)
  if (strcmp(topic, subscription_FeedMinimum) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Feed Min (Anti-Freeze) >> " + s + "\r\n");
    }
    mqttMinimumFeedTemperature = d;
  }

  //Read Ambient Temperature
  if (strcmp(topic, subscription_AmbientTemperature) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Ambient >> " + s + "\r\n");
    }
    mqttAmbientTemperature = d;
  }

  //Read Target Ambient Temperature
  if (strcmp(topic, subscription_TargetAmbientTemperature) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Ambient Target >> " + s + "\r\n");
    }
    mqttTargetAmbientTemperature = d;
  }

  //Read Target Ambient Temperature
  if (strcmp(topic, subscription_Adaption) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Adaption >> " + s + "\r\n");
    }
    mqttFeedAdaption = d;
  }

  //Read Heating on|off
  if (strcmp(topic, subscription_OnOff) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: On/Off >> " + s + "\r\n");
    }
    mqttHeatingSwitch = (i == 1 ? true : false);
    //Write value to the "control" level
    hcActive = mqttHeatingSwitch;
  }

  //Read Boost on|off
  if (strcmp(topic, subscription_OnDemandBoost) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Boost >> " + s + "\r\n");
    }
    mqttBoost = (i == 1 ? true : false);
    boostTimeCountdown = mqttBoostDuration;
    //Start Immediately.
    SetFeedTemperature();
  }

  //Read Boost Duration
  if (strcmp(topic, subscription_OnDemandBoostDuration) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Boost Duration >> " + s + "\r\n");
    }
    mqttBoostDuration = i;
  }

  //Read FastHeatup on|off
  if (strcmp(topic, subscription_FastHeatup) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Fast Heatup >> " + s + "\r\n");
    }
    mqttFastHeatup = (i == 1 ? true : false);
    //Start Immediately.
    SetFeedTemperature();
    //Set reference Temperature
    mqttReferenceAmbientTemperature = mqttAmbientTemperature;
  }

  //Read Dynamic Adaption on|off
  if (strcmp(topic, subscription_DynamicAdaption) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Dynamic Adaption >> " + s + "\r\n");
    }
    mqttDynamicAdaption = (i == 1 ? true : false);
  }

  //Read feed setpoint override on|off
  if (strcmp(topic, subscription_OverrideSetpoint) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Feed Setpoint Override >> " + s + "\r\n");
    }
    mqttOverrideSetpoint = (i == 1 ? true : false);
  }

  //Read Feed Setpint Override Temperature
  if (strcmp(topic, subscription_FeedSetpoint) == 0)
  {
    // Transform payload into a double
    double d = s.toDouble();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Feed Setpoint >> " + s + "\r\n");
    }
    mqttFeedTemperatureSetpoint = d;
  }

  //Read Valve Scaling on|off
  if (strcmp(topic, subscription_ValveScaling) == 0)
  {
    // Transform payload into a double
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Valve Scaling >> " + s + "\r\n");
    }
    mqttValveScaling = (i == 1 ? true : false);
  }

  //Read Max Valve Opening
  if (strcmp(topic, subscription_ValveScalingMaxOpening) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Max Valve Opening >> " + s + "\r\n");
    }
    mqttMaxValveOpening = i;
  }

  //Read Current Max Valve Opening
  if (strcmp(topic, subscription_ValveScalingOpening) == 0)
  {
    // Transform payload into an integer
    int i = s.toInt();
    if (Debug)
    {
      WriteToConsoles("MQTT RCV: Current Valve Opening >> " + s + "\r\n");
    }
    mqttValveOpening = i;
  }
}