#include <Arduino.h>
#include <mqtt.h>
#include <configuration.h>
#include <main.h>
#include <telnet.h>
#include <heating.h>
#include <ArduinoJson.h>
#include <ha_autodiscovery.h>
#include <failsafe.h>
#include <forecast.h>

//——————————————————————————————————————————————————————————————————————————————
//  MQTT Client (uses Wifi Client)
//——————————————————————————————————————————————————————————————————————————————
PubSubClient client(espClient);

CommandedValues commandedValues;
String TopicBuf;
String PayloadBuf;
static unsigned long lastMqttAttempt = 0;
static const unsigned long mqttRetryInterval = 30000;
static bool runtimeControlSavePending = false;
static unsigned long runtimeControlSaveRequestedAt = 0;
static const unsigned long runtimeControlSaveDelay = 3000;
static const unsigned long runtimeControlSaveRetryDelay = 30000;
static unsigned long runtimeControlSaveWait = runtimeControlSaveDelay;
static portMUX_TYPE runtimeControlMux = portMUX_INITIALIZER_UNLOCKED;
static constexpr size_t pendingControlCapacity = 1024;
static char pendingControl[pendingControlCapacity];
static size_t pendingControlLength = 0;
static uint32_t pendingControlReceivedAt = 0;
static uint32_t pendingControlForecastSubmission = 0;

namespace
{
bool readBoolean(JsonVariantConst json, const char *key, bool &target)
{
  JsonVariantConst value = json[key];
  if (value.isNull())
    return false;

  if (value.is<bool>())
  {
    target = value.as<bool>();
    return true;
  }

  // Existing Node-RED examples use JSON 0/1 for several flags.
  if (value.is<int>())
  {
    const int numeric = value.as<int>();
    if (numeric == 0 || numeric == 1)
    {
      target = numeric == 1;
      return true;
    }
  }

  return false;
}

template <typename T>
bool readConstrainedNumber(JsonVariantConst json, const char *key, T &target, double minimum, double maximum)
{
  JsonVariantConst value = json[key];
  if (value.isNull() || (!value.is<int>() && !value.is<long>() &&
                         !value.is<unsigned int>() && !value.is<unsigned long>() &&
                         !value.is<float>() && !value.is<double>()))
    return false;

  const double parsed = value.as<double>();
  if (!isfinite(parsed))
    return false;

  target = static_cast<T>(constrain(parsed, minimum, maximum));
  return true;
}

template <typename T>
bool updateIfChanged(T &destination, const T &source)
{
  if (destination == source)
    return false;
  destination = source;
  return true;
}

void scheduleRuntimeControlSave(bool stableStateChanged)
{
  if (!stableStateChanged)
    return;
  runtimeControlSavePending = true;
  runtimeControlSaveRequestedAt = millis();
  runtimeControlSaveWait = runtimeControlSaveDelay;
}
} // namespace

bool ParseForecastCommand(JsonVariantConst json, const char *correctionKey, const char *ttlKey,
                          const char *changeKey, const char *roomsKey, ForecastInput &input, bool &present)
{
  const JsonObjectConst object = json.as<JsonObjectConst>();
  const bool direct = !object[correctionKey].isUnbound();
  const bool raw = !object[changeKey].isUnbound();
  const bool ttlPresent = !object[ttlKey].isUnbound();
  const bool roomsPresent = !object[roomsKey].isUnbound();
  present = direct || raw;
  if (direct && raw)
    return false;
  if (!present)
    return !ttlPresent && !roomsPresent;
  input = ForecastInput{};
  input.TemperatureChange = raw;
  const JsonVariantConst value = object[raw ? changeKey : correctionKey];
  if (value.is<bool>() || !value.is<double>())
    return false;
  input.Value = value.as<double>();
  const double limit = raw ? 50.0 : 2.0;
  if (!isfinite(input.Value) || input.Value < -limit || input.Value > limit)
    return false;
  if (roomsPresent)
  {
    if (!object[roomsKey].is<bool>())
      return false;
    input.RoomsSatisfied = object[roomsKey].as<bool>();
  }
  if (ttlPresent)
  {
    const JsonVariantConst ttl = object[ttlKey];
    if (ttl.is<bool>() || (!ttl.is<unsigned int>() && !ttl.is<unsigned long>() && !ttl.is<int>() && !ttl.is<long>()))
      return false;
    const double seconds = ttl.as<double>();
    if (seconds < 1 || seconds > 14400)
      return false;
    input.ValidForSeconds = ttl.as<uint32_t>();
  }
  return true;
}

bool ParseForecastSettings(JsonVariantConst json, bool &enabled, bool &enabledPresent,
                           double &coefficient, bool &coefficientPresent)
{
  const JsonVariantConst flag = json["ForecastEnabled"];
  const JsonVariantConst factor = json["ForecastCoefficient"];
  enabledPresent = !flag.isUnbound();
  coefficientPresent = !factor.isUnbound();
  if (enabledPresent)
  {
    if (!flag.is<bool>())
      return false;
    enabled = flag.as<bool>();
  }
  if (coefficientPresent)
  {
    if (factor.is<bool>() || !factor.is<double>())
      return false;
    coefficient = factor.as<double>();
    if (!isfinite(coefficient) || coefficient < 0.0 || coefficient > 1.0)
      return false;
  }
  return true;
}

bool ParseWeatherSupportMaximum(JsonVariantConst json, double &maximum, bool &present)
{
  JsonVariantConst value = json["WeatherSupportMaximum"];
  present = !value.isUnbound();
  if (!present)
    return true;
  if (value.is<bool>() || !value.is<double>())
    return false;
  maximum = value.as<double>();
  return isfinite(maximum) && maximum >= 0.0 && maximum <= 100.0;
}

double GetWeatherSupportMaximum()
{
  portENTER_CRITICAL(&runtimeControlMux);
  const double maximum = commandedValues.Heating.WeatherSupportMaximum;
  portEXIT_CRITICAL(&runtimeControlMux);
  return maximum;
}

void SetWeatherSupportMaximum(double maximum)
{
  portENTER_CRITICAL(&runtimeControlMux);
  commandedValues.Heating.WeatherSupportMaximum = maximum;
  portEXIT_CRITICAL(&runtimeControlMux);
}

bool SubmitRuntimeControlCommand(JsonVariantConst json)
{
  double maximum = 60.0;
  bool maximumPresent = false;
  ForecastInput forecast;
  bool forecastPresent = false;
  bool forecastEnabled = false, enabledPresent = false, coefficientPresent = false;
  double forecastCoefficient = 0.5;
  if (!json.is<JsonObjectConst>() ||
      !ParseWeatherSupportMaximum(json, maximum, maximumPresent) ||
      !ParseForecastSettings(json, forecastEnabled, enabledPresent, forecastCoefficient, coefficientPresent) ||
      !ParseForecastCommand(json, "ForecastCorrection", "ForecastValidForSeconds",
                            "ForecastTemperatureChange", "ForecastRoomsSatisfied", forecast, forecastPresent))
    return false;

  // Normalize only accepted fields before publishing a bounded, owned command.
  // No live control reads, JSON allocation or network side effects occur under the lock.
  JsonDocument normalized;
  static const char *const booleanKeys[] = {
      "Enabled", "ValveScaling", "DynamicAdaption", "OverrideSetpoint",
      "OnDemandBoost", "Boost", "FastHeatup"};
  for (const char *key : booleanKeys)
  {
    if (json[key].isUnbound())
      continue;
    bool value = false;
    if (!readBoolean(json, key, value))
      return false;
    normalized[key] = value;
  }
  struct NumericControl { const char *key; double minimum; double maximum; };
  static const NumericControl numericControls[] = {
      {"FeedSetpoint", 0, 100}, {"FeedBaseSetpoint", -50, 50}, {"FeedCutOff", -50, 50},
      {"FeedMinimum", 0, 100}, {"AuxiliaryTemperature", -50, 100},
      {"AmbientTemperature", -50, 100}, {"TargetAmbientTemperature", 5, 35},
      {"Adaption", -30, 30}, {"ValveScalingMaxOpening", 1, 100},
      {"ValveScalingOpening", 0, 100}, {"OnDemandBoostDuration", 0, 86400},
      {"HotWaterSetpoint", 0, 100}};
  for (const NumericControl &control : numericControls)
  {
    if (json[control.key].isUnbound())
      continue;
    double value = 0.0;
    if (json[control.key].is<bool>() ||
        !readConstrainedNumber(json, control.key, value, control.minimum, control.maximum))
      return false;
    normalized[control.key] = value;
  }
  if (maximumPresent)
    normalized["WeatherSupportMaximum"] = maximum;
  if (enabledPresent)
    normalized["ForecastEnabled"] = forecastEnabled;
  if (coefficientPresent)
    normalized["ForecastCoefficient"] = forecastCoefficient;
  if (forecastPresent)
  {
    normalized[forecast.TemperatureChange ? "ForecastTemperatureChange" : "ForecastCorrection"] = forecast.Value;
    normalized["ForecastValidForSeconds"] = forecast.ValidForSeconds;
    normalized["ForecastRoomsSatisfied"] = forecast.RoomsSatisfied;
  }
  if (normalized.overflowed() || normalized.size() == 0)
    return false;
  const size_t length = measureJson(normalized);
  if (length >= pendingControlCapacity)
    return false;
  char serialized[pendingControlCapacity];
  if (serializeJson(normalized, serialized, sizeof(serialized)) != length)
    return false;
  const uint32_t forecastSubmission = forecastPresent ? ReserveForecastSubmission() : 0;
  portENTER_CRITICAL(&runtimeControlMux);
  memcpy(pendingControl, serialized, length + 1);
  pendingControlLength = length;
  pendingControlReceivedAt = millis();
  pendingControlForecastSubmission = forecastSubmission;
  portEXIT_CRITICAL(&runtimeControlMux);
  return true;
}

void ProcessPendingRuntimeControls()
{
  char serialized[pendingControlCapacity];
  portENTER_CRITICAL(&runtimeControlMux);
  const size_t length = pendingControlLength;
  const uint32_t receivedAt = pendingControlReceivedAt;
  const uint32_t forecastSubmission = pendingControlForecastSubmission;
  if (length)
  {
    memcpy(serialized, pendingControl, length + 1);
    pendingControlLength = 0;
  }
  portEXIT_CRITICAL(&runtimeControlMux);
  if (!length)
    return;
  JsonDocument command;
  if (deserializeJson(command, serialized, length))
    return;
  ApplyHeatingCommand(command.as<JsonVariantConst>(), false, receivedAt, forecastSubmission);
  if (!command["HotWaterSetpoint"].isUnbound())
  {
    JsonDocument water;
    water["Setpoint"] = command["HotWaterSetpoint"];
    ApplyHotWaterCommand(water.as<JsonVariantConst>(), false);
  }
}

bool ApplyHeatingCommand(JsonVariantConst json, bool persistStableState, uint32_t forecastReceivedAtMillis, uint32_t forecastSubmission)
{
  ForecastInput forecast;
  bool forecastPresent = false;
  bool forecastEnabled = false, enabledPresent = false, coefficientPresent = false;
  double forecastCoefficient = 0.5;
  double weatherSupportMaximum = 60.0;
  bool weatherSupportPresent = false;
  if (!json.is<JsonObjectConst>() ||
      !ParseWeatherSupportMaximum(json, weatherSupportMaximum, weatherSupportPresent) ||
      !ParseForecastSettings(json, forecastEnabled, enabledPresent, forecastCoefficient, coefficientPresent) ||
      !ParseForecastCommand(json, "ForecastCorrection", "ForecastValidForSeconds",
                            "ForecastTemperatureChange", "ForecastRoomsSatisfied", forecast, forecastPresent))
    return false;
  const bool forecastQueued = forecastPresent && SubmitForecast(forecast, forecastReceivedAtMillis, forecastSubmission);
  bool changed = false;
  bool stableStateChanged = false;
  if (enabledPresent || coefficientPresent)
  {
    const ForecastState current = GetForecastState();
    const bool newEnabled = enabledPresent ? forecastEnabled : current.Enabled;
    const double newCoefficient = coefficientPresent ? forecastCoefficient : current.Coefficient;
    SetForecastSettings(newEnabled, newCoefficient);
    changed = true;
    if (persistStableState)
    {
      if (enabledPresent)
        stableStateChanged |= updateIfChanged(configuration.RuntimeControls.ForecastEnabled, newEnabled);
      if (coefficientPresent)
        stableStateChanged |= updateIfChanged(configuration.RuntimeControls.ForecastCoefficient, newCoefficient);
    }
  }
  if (weatherSupportPresent)
  {
    SetWeatherSupportMaximum(weatherSupportMaximum);
    changed = true;
    if (persistStableState)
      stableStateChanged |= updateIfChanged(configuration.RuntimeControls.WeatherSupportMaximum, weatherSupportMaximum);
  }
  bool accepted = readBoolean(json, "Enabled", commandedValues.Heating.Active);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.HeatingEnabled, commandedValues.Heating.Active);
  accepted = readConstrainedNumber(json, "FeedSetpoint", commandedValues.Heating.FeedSetpoint, 0.0, 100.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.FeedSetpoint, commandedValues.Heating.FeedSetpoint);
  accepted = readConstrainedNumber(json, "FeedBaseSetpoint", commandedValues.Heating.BasepointTemperature, -50.0, 50.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.General.BasepointTemperature, commandedValues.Heating.BasepointTemperature);
  accepted = readConstrainedNumber(json, "FeedCutOff", commandedValues.Heating.EndpointTemperature, -50.0, 50.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.General.EndpointTemperature, commandedValues.Heating.EndpointTemperature);
  accepted = readConstrainedNumber(json, "FeedMinimum", commandedValues.Heating.MinimumFeedTemperature, 0.0, 100.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.MinimumFeedTemperature, commandedValues.Heating.MinimumFeedTemperature);
  changed |= readConstrainedNumber(json, "AuxiliaryTemperature", commandedValues.Heating.AuxiliaryTemperature, -50.0, 100.0);
  changed |= readConstrainedNumber(json, "AmbientTemperature", commandedValues.Heating.AmbientTemperature, -50.0, 100.0);
  accepted = readConstrainedNumber(json, "TargetAmbientTemperature", commandedValues.Heating.TargetAmbientTemperature, 5.0, 35.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.TargetAmbientTemperature, commandedValues.Heating.TargetAmbientTemperature);
  accepted = readConstrainedNumber(json, "Adaption", commandedValues.Heating.FeedAdaption, -30.0, 30.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.FeedAdaption, commandedValues.Heating.FeedAdaption);
  accepted = readBoolean(json, "ValveScaling", commandedValues.Heating.ValveScaling);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.ValveScaling, commandedValues.Heating.ValveScaling);
  accepted = readConstrainedNumber(json, "ValveScalingMaxOpening", commandedValues.Heating.MaxValveOpening, 1.0, 100.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.MaxValveOpening, commandedValues.Heating.MaxValveOpening);
  changed |= readConstrainedNumber(json, "ValveScalingOpening", commandedValues.Heating.ValveOpening, 0.0, 100.0);
  accepted = readBoolean(json, "DynamicAdaption", commandedValues.Heating.DynamicAdaption);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.DynamicAdaption, commandedValues.Heating.DynamicAdaption);
  accepted = readBoolean(json, "OverrideSetpoint", commandedValues.Heating.OverrideSetpoint);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.OverrideSetpoint, commandedValues.Heating.OverrideSetpoint);
  accepted = readConstrainedNumber(json, "OnDemandBoostDuration", commandedValues.Heating.BoostDuration, 0.0, 86400.0);
  changed |= accepted;
  if (persistStableState && accepted)
    stableStateChanged |= updateIfChanged(configuration.RuntimeControls.BoostDuration, commandedValues.Heating.BoostDuration);

  bool boost = commandedValues.Heating.Boost;
  if (readBoolean(json, "OnDemandBoost", boost) || readBoolean(json, "Boost", boost))
  {
    commandedValues.Heating.Boost = boost;
    commandedValues.Heating.BoostTimeCountdown = boost ? commandedValues.Heating.BoostDuration : 0;
    changed = true;
  }

  bool fastHeatup = commandedValues.Heating.FastHeatup;
  if (readBoolean(json, "FastHeatup", fastHeatup))
  {
    if (fastHeatup && !commandedValues.Heating.FastHeatup)
      commandedValues.Heating.ReferenceAmbientTemperature = commandedValues.Heating.AmbientTemperature;
    commandedValues.Heating.FastHeatup = fastHeatup;
    changed = true;
  }

  if (changed)
  {
    if (persistStableState)
      scheduleRuntimeControlSave(stableStateChanged);
    NotifyValidHeatingCommand();
    SetFeedTemperature();
    PublishHeatingTemperaturesAndStatus();
  }
  return changed || forecastQueued;
}

bool ApplyHotWaterCommand(JsonVariantConst json, bool persistStableState)
{
  if (!readConstrainedNumber(json, "Setpoint", commandedValues.HotWater.SetPoint, 0.0, 100.0))
    return false;
  if (persistStableState)
    scheduleRuntimeControlSave(updateIfChanged(configuration.RuntimeControls.HotWaterSetpoint, commandedValues.HotWater.SetPoint));
  PublishWaterTemperatures();
  return true;
}

void ProcessRuntimeControlPersistence()
{
  if (!runtimeControlSavePending || millis() - runtimeControlSaveRequestedAt < runtimeControlSaveWait)
    return;

  if (WriteConfiguration())
  {
    runtimeControlSavePending = false;
    runtimeControlSaveWait = runtimeControlSaveDelay;
    Log.println("Persisted MQTT runtime control state.");
  }
  else
  {
    runtimeControlSaveRequestedAt = millis();
    runtimeControlSaveWait = runtimeControlSaveRetryDelay;
    Log.println("MQTT runtime control state save failed; retry scheduled.");
  }
}

void ApplyBoostCommand(bool enabled)
{
  commandedValues.Heating.Boost = enabled;
  commandedValues.Heating.BoostTimeCountdown = enabled ? commandedValues.Heating.BoostDuration : 0;
  NotifyValidHeatingCommand();
  SetFeedTemperature();
  PublishHeatingTemperaturesAndStatus();
}

void ApplyFastHeatupCommand(bool enabled)
{
  commandedValues.Heating.FastHeatup = enabled;
  if (enabled)
    commandedValues.Heating.ReferenceAmbientTemperature = commandedValues.Heating.AmbientTemperature;
  NotifyValidHeatingCommand();
  SetFeedTemperature();
  PublishHeatingTemperaturesAndStatus();
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
  // Establish TCP with an explicit deadline. PubSubClient reuses this socket
  // instead of invoking the core's default-timeout connect overload.
  if (!espClient.connect(configuration.Mqtt.Server, configuration.Mqtt.Port, 250))
  {
    Log.printf("TCP connection failed. Retrying in %lu seconds while CAN control continues.\r\n",
               mqttRetryInterval / 1000);
    return;
  }

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
  // The TCP connect is bounded explicitly in reconnectMqtt; bound CONNACK too.
  client.setSocketTimeout(1);
  // Device discovery is intentionally emitted as one Home Assistant device
  // document. The complete command surface currently serializes to about
  // 20 KiB, so leave enough room for the MQTT topic and future components.
  if (!client.setBufferSize(24576))
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

    ApplyHeatingCommand(doc.as<JsonVariantConst>(), true);

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

      ApplyHotWaterCommand(doc.as<JsonVariantConst>(), true);
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
  jsonObj["HasFeedMaximum"] = ceraValues.Heating.HasReceivedFeedMaximum;
  jsonObj["FeedCurrent"] = ceraValues.Heating.FeedCurrent;
  if (OverrideControl)
    jsonObj["FeedSetpoint"] = commandedValues.Heating.CalculatedFeedSetpoint;
  else if (ceraValues.Heating.HasReceivedFeedSetpoint)
    jsonObj["FeedSetpoint"] = ceraValues.Heating.FeedSetpoint;
  else
    jsonObj["FeedSetpoint"] = nullptr;
  jsonObj["Outside"] = ceraValues.General.OutsideTemperature;
  jsonObj["Pump"] = boolToString(ceraValues.Heating.PumpActive);
  jsonObj["Season"] = boolToString(ceraValues.Heating.Season);
  jsonObj["Working"] = boolToString(ceraValues.Heating.Active);
  jsonObj["Boost"] = boolToString(commandedValues.Heating.Boost);
  jsonObj["BoostTimeLeft"] = commandedValues.Heating.BoostTimeCountdown;
  jsonObj["FastHeatup"] = boolToString(commandedValues.Heating.FastHeatup);
  jsonObj["Enabled"] = boolToString(commandedValues.Heating.Active);
  jsonObj["FeedBaseSetpoint"] = commandedValues.Heating.BasepointTemperature;
  jsonObj["FeedCutOff"] = commandedValues.Heating.EndpointTemperature;
  jsonObj["FeedMinimum"] = commandedValues.Heating.MinimumFeedTemperature;
  jsonObj["AuxiliaryTemperature"] = commandedValues.Heating.AuxiliaryTemperature;
  jsonObj["AmbientTemperature"] = commandedValues.Heating.AmbientTemperature;
  jsonObj["TargetAmbientTemperature"] = commandedValues.Heating.TargetAmbientTemperature;
  jsonObj["Adaption"] = commandedValues.Heating.FeedAdaption;
  jsonObj["WeatherSupportMaximum"] = GetWeatherSupportMaximum();
  jsonObj["ValveScaling"] = boolToString(commandedValues.Heating.ValveScaling);
  jsonObj["ValveScalingMaxOpening"] = commandedValues.Heating.MaxValveOpening;
  jsonObj["ValveScalingOpening"] = commandedValues.Heating.ValveOpening;
  jsonObj["DynamicAdaption"] = boolToString(commandedValues.Heating.DynamicAdaption);
  jsonObj["OverrideSetpoint"] = boolToString(commandedValues.Heating.OverrideSetpoint);
  jsonObj["OnDemandBoostDuration"] = commandedValues.Heating.BoostDuration;
  const ForecastState forecast = GetForecastState();
  jsonObj["ForecastCorrection"] = forecast.Correction;
  jsonObj["ForecastRemainingSeconds"] = forecast.RemainingSeconds;
  jsonObj["ForecastEnabled"] = forecast.Enabled;
  jsonObj["ForecastCoefficient"] = forecast.Coefficient;
  jsonObj["ForecastRoomsSatisfied"] = forecast.RoomsSatisfied;
  if (forecast.HasTemperatureChange)
    jsonObj["ForecastTemperatureChange"] = forecast.TemperatureChange;

  // Compatibility aliases retained for existing dashboards and discovery
  // documents that predate the complete command-state mirror.
  jsonObj["RequestedFeedSetpoint"] = commandedValues.Heating.FeedSetpoint;
  jsonObj["BoostDuration"] = commandedValues.Heating.BoostDuration;
  jsonObj["RoomReferenceT"] = commandedValues.Heating.AmbientTemperature;

  // Mute Flag Set. Don't send message.
  if (MUTE_MQTT == 1)
    return;

  // Publish Data on MQTT
  char buffer[1536];
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
  if (OverrideControl)
    jsonObj["Setpoint"] = commandedValues.HotWater.SetPoint;
  else if (ceraValues.Hotwater.HasReceivedSetPoint)
    jsonObj["Setpoint"] = ceraValues.Hotwater.SetPoint;
  else
    jsonObj["Setpoint"] = nullptr;

  if (ceraValues.Hotwater.HasReceivedContinousFlowSetpoint)
    jsonObj["CFSetpoint"] = ceraValues.Hotwater.ContinousFlowSetpoint;
  else
    jsonObj["CFSetpoint"] = nullptr;
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
