#include <Arduino.h>
#include <ArduinoJson.h>
#include <feed_temperature.h>
#include <math.h>
#include <string.h>

namespace
{
constexpr size_t inputCapacity = 512;
char inputLine[inputCapacity];
size_t inputLength = 0;
bool inputOverflow = false;

bool readBoolean(JsonObjectConst object, const char *name, bool &destination, String &error)
{
    const JsonVariantConst value = object[name];
    if (value.isUnbound())
        return true;
    if (!value.is<bool>())
    {
        error = String(name) + " must be a boolean";
        return false;
    }
    destination = value.as<bool>();
    return true;
}

bool readNumber(JsonObjectConst object, const char *name, double &destination, String &error)
{
    const JsonVariantConst value = object[name];
    if (value.isUnbound())
        return true;
    if (!value.is<double>())
    {
        error = String(name) + " must be a number";
        return false;
    }
    destination = value.as<double>();
    return true;
}

bool parseInputs(JsonObjectConst object, FeedTemperatureInputs &inputs, String &error)
{
    static const char *const fields[] = {
        "Enabled", "OverrideSetpoint", "Boost", "ValveScaling", "DynamicAdaption",
        "FastHeatup", "OutsideTemperatureValid", "HoldMinimumTemperature", "FeedSetpoint", "FeedMaximum",
        "MinimumFeedTemperature", "BasepointTemperature", "EndpointTemperature",
        "OutsideTemperature", "FeedAdaption", "ValveOpening", "MaxValveOpening",
        "WeatherSupportMaximum",
        "AmbientTemperature", "TargetAmbientTemperature", "ReferenceAmbientTemperature",
        "FeedReturnTemperatureReference", "ForecastCorrection", "ForecastEnabled",
        "ForecastCoefficient", "ForecastTemperatureChange", "ForecastRoomsSatisfied"};
    for (const JsonPairConst pair : object)
    {
        bool recognized = false;
        for (const char *field : fields)
        {
            if (strcmp(pair.key().c_str(), field) == 0)
            {
                recognized = true;
                break;
            }
        }
        if (!recognized)
        {
            error = String("unknown input field: ") + pair.key().c_str();
            return false;
        }
    }

#define READ_BOOL(field) if (!readBoolean(object, #field, inputs.field, error)) return false
#define READ_NUMBER(field) if (!readNumber(object, #field, inputs.field, error)) return false
    READ_BOOL(Enabled);
    READ_BOOL(OverrideSetpoint);
    READ_BOOL(Boost);
    READ_BOOL(ValveScaling);
    READ_BOOL(DynamicAdaption);
    READ_BOOL(FastHeatup);
    READ_BOOL(OutsideTemperatureValid);
    READ_BOOL(HoldMinimumTemperature);
    READ_NUMBER(FeedSetpoint);
    READ_NUMBER(FeedMaximum);
    READ_NUMBER(MinimumFeedTemperature);
    READ_NUMBER(BasepointTemperature);
    READ_NUMBER(EndpointTemperature);
    READ_NUMBER(OutsideTemperature);
    READ_NUMBER(FeedAdaption);
    READ_NUMBER(WeatherSupportMaximum);
    if (!isfinite(inputs.WeatherSupportMaximum) ||
        inputs.WeatherSupportMaximum < 0.0 || inputs.WeatherSupportMaximum > 100.0)
    {
        error = "WeatherSupportMaximum must be a finite number from 0 to 100";
        return false;
    }
    READ_NUMBER(ValveOpening);
    READ_NUMBER(MaxValveOpening);
    READ_NUMBER(AmbientTemperature);
    READ_NUMBER(TargetAmbientTemperature);
    READ_NUMBER(ReferenceAmbientTemperature);
    READ_NUMBER(FeedReturnTemperatureReference);
    READ_NUMBER(ForecastCorrection);
    READ_BOOL(ForecastRoomsSatisfied);
    bool forecastEnabled = false;
    double forecastCoefficient = 0.5, temperatureChange = 0.0;
    if (!readBoolean(object, "ForecastEnabled", forecastEnabled, error) ||
        !readNumber(object, "ForecastCoefficient", forecastCoefficient, error) ||
        !readNumber(object, "ForecastTemperatureChange", temperatureChange, error))
        return false;
    if (!isfinite(forecastCoefficient) || forecastCoefficient < 0.0 || forecastCoefficient > 1.0)
    {
        error = "ForecastCoefficient must be a finite number from 0 to 1";
        return false;
    }
    const bool rawForecast = !object["ForecastTemperatureChange"].isUnbound();
    if (rawForecast && !object["ForecastCorrection"].isUnbound())
    {
        error = "ForecastTemperatureChange and ForecastCorrection are mutually exclusive";
        return false;
    }
    if (rawForecast)
    {
        if (!isfinite(temperatureChange) || temperatureChange < -50.0 || temperatureChange > 50.0)
        {
            error = "ForecastTemperatureChange must be a finite number from -50 to 50";
            return false;
        }
        inputs.ForecastCorrection = forecastEnabled ? DeriveForecastCorrection(temperatureChange, forecastCoefficient) : 0.0;
    }
    else if (!object["ForecastEnabled"].isUnbound() && !forecastEnabled)
        inputs.ForecastCorrection = 0.0;
#undef READ_BOOL
#undef READ_NUMBER
    return true;
}

void writeError(const char *message)
{
    JsonDocument response;
    response["error"] = message;
    serializeJson(response, Serial);
    Serial.println();
}

void processLine()
{
    if (inputOverflow)
    {
        writeError("input exceeds 511 bytes");
        return;
    }

    JsonDocument request;
    const DeserializationError parseError = deserializeJson(request, inputLine, inputLength);
    if (parseError)
    {
        writeError(parseError.c_str());
        return;
    }
    if (!request.is<JsonObject>())
    {
        writeError("input must be a JSON object");
        return;
    }

    FeedTemperatureInputs inputs;
    String error;
    if (!parseInputs(request.as<JsonObjectConst>(), inputs, error))
    {
        writeError(error.c_str());
        return;
    }

    const FeedTemperatureResult result = CalculateFeedTemperatureForInputs(inputs);
    JsonDocument response;
    response["Setpoint"] = result.Setpoint;
    response["DisableFastHeatup"] = result.DisableFastHeatup;
    serializeJson(response, Serial);
    Serial.println();
}
} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("{\"ready\":true}");
}

void loop()
{
    while (Serial.available() > 0)
    {
        const char value = static_cast<char>(Serial.read());
        if (value == '\n')
        {
            inputLine[inputLength] = '\0';
            processLine();
            inputLength = 0;
            inputOverflow = false;
        }
        else if (!inputOverflow)
        {
            if (inputLength + 1 < inputCapacity)
                inputLine[inputLength++] = value;
            else
                inputOverflow = true;
        }
    }
}
