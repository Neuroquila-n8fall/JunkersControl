#include <Arduino.h>
#include <unity.h>
#include <ArduinoJson.h>
#include <mqtt.h>
#include <forecast.h>
#include <feed_temperature.h>
#include <main.h>
#include <math.h>
#include <string.h>

namespace
{
FeedTemperatureInputs heatingInputs;

bool SubmitText(const char *text)
{
    JsonDocument document;
    if (deserializeJson(document, text))
        return false;
    return SubmitRuntimeControlCommand(document.as<JsonVariantConst>());
}

void AssertNear(double actual, double expected)
{
    TEST_ASSERT_TRUE(fabs(actual - expected) <= 0.001);
}

void MixedMalformedCommandCannotPartiallyApplyOrReplacePendingCommand()
{
    TEST_ASSERT_TRUE(SubmitText("{\"Enabled\":false,\"FeedSetpoint\":42}"));
    TEST_ASSERT_FALSE(SubmitText("{\"Enabled\":\"false\",\"OverrideSetpoint\":true,\"FeedSetpoint\":75}"));

    ProcessPendingRuntimeControls();
    TEST_ASSERT_FALSE(commandedValues.Heating.Active);
    AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);
    TEST_ASSERT_FALSE(commandedValues.Heating.OverrideSetpoint);
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 10.0);
}

void EveryMalformedPresentFieldRejectsTheWholeDocumentAndPreservesQueuedState()
{
    const char *malformed[] = {
        "{\"Enabled\":\"false\"}",
        "{\"Enabled\":null}",
        "{\"FeedSetpoint\":\"42\"}",
        "{\"FeedSetpoint\":null}",
        "{\"FeedSetpoint\":true}",
        "{\"Enabled\":2}",
    };

    for (const char *text : malformed)
    {
        TEST_ASSERT_TRUE(SubmitText("{\"FeedSetpoint\":42}"));
        TEST_ASSERT_FALSE(SubmitText(text));
        ProcessPendingRuntimeControls();
        AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);
        TEST_ASSERT_TRUE(commandedValues.Heating.Active);
    }

    JsonDocument nonFinite;
    nonFinite["FeedSetpoint"] = NAN;
    TEST_ASSERT_FALSE(SubmitRuntimeControlCommand(nonFinite.as<JsonVariantConst>()));
    TEST_ASSERT_TRUE(SubmitText("{\"FeedSetpoint\":42}"));
    ProcessPendingRuntimeControls();
    AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);
}

void MalformedRuntimeDocumentCannotReplaceQueuedForecastSample()
{
    TEST_ASSERT_TRUE(SubmitText(
        "{\"Enabled\":false,\"FeedSetpoint\":42,\"ForecastEnabled\":true,\"ForecastCoefficient\":0.5,\"ForecastCorrection\":1.25,\"ForecastValidForSeconds\":300,\"ForecastRoomsSatisfied\":true}"));
    TEST_ASSERT_FALSE(SubmitText(
        "{\"Enabled\":true,\"FeedSetpoint\":75,\"ForecastCorrection\":-1.5,\"ForecastValidForSeconds\":300,\"ForecastRoomsSatisfied\":false,\"OverrideSetpoint\":null}"));

    ProcessPendingRuntimeControls();
    ProcessForecastCorrection();

    TEST_ASSERT_FALSE(commandedValues.Heating.Active);
    AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);
    const ForecastState forecast = GetForecastState();
    TEST_ASSERT_TRUE(forecast.Enabled);
    TEST_ASSERT_TRUE(forecast.RoomsSatisfied);
    TEST_ASSERT_EQUAL_UINT32(300, forecast.RemainingSeconds);
    AssertNear(forecast.Correction, 1.25);
    TEST_ASSERT_FALSE(forecast.HasTemperatureChange);
}

void EmptyAndUnknownOnlyDocumentsRejectButUnknownFieldsDoNotBlockRecognizedFields()
{
    TEST_ASSERT_FALSE(SubmitText("{}"));
    TEST_ASSERT_FALSE(SubmitText("{\"NotAControl\":17}"));
    TEST_ASSERT_TRUE(SubmitText("{\"NotAControl\":17,\"FeedSetpoint\":42}"));
    ProcessPendingRuntimeControls();
    AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);

    TEST_ASSERT_TRUE(SubmitText("{\"Enabled\":false}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_FALSE(commandedValues.Heating.Active);
    AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);
}

void BooleanRepresentationsAndExactNumericBoundsAreAccepted()
{
    TEST_ASSERT_TRUE(SubmitText("{\"Enabled\":false,\"FeedSetpoint\":0,\"ValveScalingOpening\":0}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_FALSE(commandedValues.Heating.Active);
    AssertNear(commandedValues.Heating.FeedSetpoint, 0.0);
    AssertNear(commandedValues.Heating.ValveOpening, 0.0);

    TEST_ASSERT_TRUE(SubmitText("{\"Enabled\":0,\"FeedSetpoint\":100,\"ValveScalingOpening\":100}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_FALSE(commandedValues.Heating.Active);
    AssertNear(commandedValues.Heating.FeedSetpoint, 100.0);
    AssertNear(commandedValues.Heating.ValveOpening, 100.0);

    TEST_ASSERT_TRUE(SubmitText("{\"Enabled\":1}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_TRUE(commandedValues.Heating.Active);
    TEST_ASSERT_TRUE(SubmitText("{\"Enabled\":true}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_TRUE(commandedValues.Heating.Active);
}

void NewValidDocumentReplacesRatherThanMergesPendingControls()
{
    TEST_ASSERT_TRUE(SubmitText("{\"Enabled\":false,\"FeedSetpoint\":42}"));
    TEST_ASSERT_TRUE(SubmitText("{\"FeedSetpoint\":37}"));
    ProcessPendingRuntimeControls();

    TEST_ASSERT_TRUE(commandedValues.Heating.Active);
    AssertNear(commandedValues.Heating.FeedSetpoint, 37.0);
}

void InvalidForecastRangeRejectsOtherwiseValidOrdinaryControls()
{
    TEST_ASSERT_TRUE(SubmitText("{\"FeedSetpoint\":42}"));
    TEST_ASSERT_FALSE(SubmitText(
        "{\"Enabled\":false,\"FeedSetpoint\":75,\"ForecastCorrection\":2.1,\"ForecastValidForSeconds\":300}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_TRUE(commandedValues.Heating.Active);
    AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);
    TEST_ASSERT_FALSE(GetForecastState().Enabled);
}

void OutOfRangeNumericRejectsDocumentAndPreservesPendingForecastAndControls()
{
    TEST_ASSERT_TRUE(SubmitText(
        "{\"FeedSetpoint\":42,\"ForecastCorrection\":1.25,\"ForecastValidForSeconds\":300,\"ForecastRoomsSatisfied\":true}"));
    TEST_ASSERT_FALSE(SubmitText(
        "{\"Enabled\":false,\"ValveScalingOpening\":1000,\"ForecastCorrection\":-1.5,\"ForecastValidForSeconds\":300}"));
    ProcessPendingRuntimeControls();
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.FeedSetpoint, 42.0);
    TEST_ASSERT_TRUE(commandedValues.Heating.Active);
    AssertNear(GetForecastState().Correction, 1.25);
    TEST_ASSERT_TRUE(GetForecastState().RoomsSatisfied);
}

void LegacyHeatingNumbersStillClampToBounds()
{
    JsonDocument document;
    document["FeedSetpoint"] = -5;
    TEST_ASSERT_TRUE(ApplyHeatingCommand(document.as<JsonVariantConst>()));
    AssertNear(commandedValues.Heating.FeedSetpoint, 0.0);
    document["FeedSetpoint"] = 105;
    TEST_ASSERT_TRUE(ApplyHeatingCommand(document.as<JsonVariantConst>()));
    AssertNear(commandedValues.Heating.FeedSetpoint, 100.0);
}

void RepeatedBoostDoesNotRestartCountdownAndDisabledBoostClearsIt()
{
    commandedValues.Heating.BoostDuration = 300;
    TEST_ASSERT_TRUE(SubmitText("{\"Boost\":true}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_EQUAL_UINT32(300, commandedValues.Heating.BoostTimeCountdown);
    commandedValues.Heating.BoostTimeCountdown = 123;
    TEST_ASSERT_TRUE(SubmitText("{\"OnDemandBoost\":true}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_EQUAL_UINT32(123, commandedValues.Heating.BoostTimeCountdown);
    ApplyBoostCommand(true);
    TEST_ASSERT_EQUAL_UINT32(123, commandedValues.Heating.BoostTimeCountdown);
    TEST_ASSERT_TRUE(SubmitText("{\"Boost\":false}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_FALSE(commandedValues.Heating.Boost);
    TEST_ASSERT_EQUAL_UINT32(0, commandedValues.Heating.BoostTimeCountdown);
    ApplyBoostCommand(true);
    TEST_ASSERT_EQUAL_UINT32(300, commandedValues.Heating.BoostTimeCountdown);
    ApplyBoostCommand(false);
    TEST_ASSERT_EQUAL_UINT32(0, commandedValues.Heating.BoostTimeCountdown);
}

void RepeatedFastHeatupKeepsOriginalAmbientReference()
{
    commandedValues.Heating.AmbientTemperature = 18.0;
    ApplyFastHeatupCommand(true);
    AssertNear(commandedValues.Heating.ReferenceAmbientTemperature, 18.0);
    commandedValues.Heating.AmbientTemperature = 20.0;
    ApplyFastHeatupCommand(true);
    TEST_ASSERT_TRUE(SubmitText("{\"FastHeatup\":true}"));
    ProcessPendingRuntimeControls();
    AssertNear(commandedValues.Heating.ReferenceAmbientTemperature, 18.0);
    TEST_ASSERT_TRUE(SubmitText("{\"FastHeatup\":false}"));
    ProcessPendingRuntimeControls();
    TEST_ASSERT_FALSE(commandedValues.Heating.FastHeatup);
    TEST_ASSERT_TRUE(SubmitText("{\"FastHeatup\":true}"));
    ProcessPendingRuntimeControls();
    AssertNear(commandedValues.Heating.ReferenceAmbientTemperature, 20.0);
}

void AuxiliaryTemperatureBecomesValidOnlyAfterAcceptedSample()
{
    TEST_ASSERT_FALSE(commandedValues.Heating.HasReceivedAuxiliaryTemperature);
    JsonDocument document;
    document["AuxiliaryTemperature"] = "invalid";
    TEST_ASSERT_FALSE(ApplyHeatingCommand(document.as<JsonVariantConst>()));
    TEST_ASSERT_FALSE(commandedValues.Heating.HasReceivedAuxiliaryTemperature);
    TEST_ASSERT_FALSE(SubmitText("{\"AuxiliaryTemperature\":101}"));
    TEST_ASSERT_FALSE(commandedValues.Heating.HasReceivedAuxiliaryTemperature);
    TEST_ASSERT_TRUE(SubmitText("{\"AuxiliaryTemperature\":0}"));
    TEST_ASSERT_FALSE(commandedValues.Heating.HasReceivedAuxiliaryTemperature);
    ProcessPendingRuntimeControls();
    TEST_ASSERT_TRUE(commandedValues.Heating.HasReceivedAuxiliaryTemperature);
    AssertNear(commandedValues.Heating.AuxiliaryTemperature, 0.0);
}
} // namespace

volatile bool SetupMode = false;
bool OverrideControl = false;
TaskHandle_t MqttActivityHandle = reinterpret_cast<TaskHandle_t>(1);
CeraValues ceraValues;
Configuration configuration;
WiFiClient espClient;
WiFiClient TelnetRemoteClient;
ConsoleWriter Log;

void ShowMqttActivity(void *) {}
bool WriteConfiguration() { return false; }
void NotifyValidHeatingCommand() {}

double CalculateFeedTemperature()
{
    const ForecastState forecast = GetForecastState();
    heatingInputs.Enabled = commandedValues.Heating.Active;
    heatingInputs.OverrideSetpoint = commandedValues.Heating.OverrideSetpoint;
    heatingInputs.Boost = commandedValues.Heating.Boost;
    heatingInputs.ValveScaling = commandedValues.Heating.ValveScaling;
    heatingInputs.DynamicAdaption = commandedValues.Heating.DynamicAdaption;
    heatingInputs.FastHeatup = commandedValues.Heating.FastHeatup;
    heatingInputs.FeedSetpoint = commandedValues.Heating.FeedSetpoint;
    heatingInputs.MinimumFeedTemperature = commandedValues.Heating.MinimumFeedTemperature;
    heatingInputs.BasepointTemperature = commandedValues.Heating.BasepointTemperature;
    heatingInputs.EndpointTemperature = commandedValues.Heating.EndpointTemperature;
    heatingInputs.FeedAdaption = commandedValues.Heating.FeedAdaption;
    heatingInputs.WeatherSupportMaximum = GetWeatherSupportMaximum();
    heatingInputs.ValveOpening = commandedValues.Heating.ValveOpening;
    heatingInputs.MaxValveOpening = commandedValues.Heating.MaxValveOpening;
    heatingInputs.AmbientTemperature = commandedValues.Heating.AmbientTemperature;
    heatingInputs.TargetAmbientTemperature = commandedValues.Heating.TargetAmbientTemperature;
    heatingInputs.ForecastCorrection = GetForecastCorrection(millis());
    heatingInputs.ForecastRoomsSatisfied = forecast.Enabled && forecast.RoomsSatisfied;
    return CalculateFeedTemperatureForInputs(heatingInputs).Setpoint;
}

void SetFeedTemperature()
{
    commandedValues.Heating.CalculatedFeedSetpoint = CalculateFeedTemperature();
}

void setUp()
{
    ProcessPendingRuntimeControls();
    commandedValues = CommandedValues{};
    heatingInputs = FeedTemperatureInputs{};
    heatingInputs.OutsideTemperature = 10.0;
    SetForecastSettings(false, 0.5);
    TEST_ASSERT_TRUE(SubmitForecast({0.0, 1, false, false}));
    ProcessForecastCorrection();
    SetFeedTemperature();
}

void tearDown() {}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(MixedMalformedCommandCannotPartiallyApplyOrReplacePendingCommand);
    RUN_TEST(EveryMalformedPresentFieldRejectsTheWholeDocumentAndPreservesQueuedState);
    RUN_TEST(MalformedRuntimeDocumentCannotReplaceQueuedForecastSample);
    RUN_TEST(EmptyAndUnknownOnlyDocumentsRejectButUnknownFieldsDoNotBlockRecognizedFields);
    RUN_TEST(BooleanRepresentationsAndExactNumericBoundsAreAccepted);
    RUN_TEST(NewValidDocumentReplacesRatherThanMergesPendingControls);
    RUN_TEST(InvalidForecastRangeRejectsOtherwiseValidOrdinaryControls);
    RUN_TEST(OutOfRangeNumericRejectsDocumentAndPreservesPendingForecastAndControls);
    RUN_TEST(LegacyHeatingNumbersStillClampToBounds);
    RUN_TEST(RepeatedBoostDoesNotRestartCountdownAndDisabledBoostClearsIt);
    RUN_TEST(RepeatedFastHeatupKeepsOriginalAmbientReference);
    RUN_TEST(AuxiliaryTemperatureBecomesValidOnlyAfterAcceptedSample);
    UNITY_END();
}

void loop() {}
