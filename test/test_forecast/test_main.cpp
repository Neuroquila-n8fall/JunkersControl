#include <Arduino.h>
#include <unity.h>
#include <forecast.h>
#include <feed_temperature.h>
#include <main.h>
#include <mqtt.h>
#include <math.h>

// Only CAN/network side effects are disconnected. Tests execute the real
// forecast state machine, clock/critical sections and temperature calculation.
volatile bool SetupMode = false;
ForecastTestCommandedValues commandedValues;

namespace
{
FeedTemperatureInputs heatingInputs;
void AssertNear(double actual, double expected)
{
    TEST_ASSERT_TRUE(fabs(actual - expected) <= 0.001);
}
void ApplySample(double change, uint32_t lifetime, bool satisfied = false)
{
    TEST_ASSERT_TRUE(SubmitForecast({change, lifetime, true, satisfied}));
    ProcessForecastCorrection();
}
void AvailableSampleDoesNotHeatWhenDisabled()
{
    ApplySample(-4.0, 60);
    const ForecastState state = GetForecastState();
    TEST_ASSERT_FALSE(state.Enabled);
    AssertNear(state.Correction, 2.0);
    AssertNear(GetForecastCorrection(millis()), 0.0);
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
}
void EnableAndCoefficientChangesRecalculateFreshSample()
{
    ApplySample(-4.0, 60);
    SetForecastSettings(true, 0.5);
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 59.0);
    SetForecastSettings(true, 0.25);
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 58.0);
    SetForecastSettings(true, 0.0);
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
    SetForecastSettings(false, 0.5);
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
}
void RoomPermissionChangesRecalculateWithoutCorrectionChange()
{
    SetForecastSettings(true, 0.5);
    ApplySample(4.0, 60, false);
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
    ApplySample(4.0, 60, true);
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 55.0);
    ApplySample(4.0, 60, false);
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
}
void ExpiryDropsCorrectionAndRoomPermission()
{
    SetForecastSettings(true, 0.5);
    ApplySample(4.0, 1, true);
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 55.0);
    delay(1100);
    AssertNear(GetForecastCorrection(millis()), 0.0);
    TEST_ASSERT_FALSE(GetForecastState().RoomsSatisfied);
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
    TEST_ASSERT_EQUAL_UINT32(0, GetForecastState().RemainingSeconds);
}
void LifetimeStartsAtReceiptNotApplication()
{
    SetForecastSettings(true, 0.5);
    TEST_ASSERT_TRUE(SubmitForecast({-4.0, 1, true, false}));
    delay(1100);
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
    AssertNear(GetForecastState().Correction, 0.0);
    TEST_ASSERT_EQUAL_UINT32(0, GetForecastState().RemainingSeconds);
}
void NewestPendingSampleWinsAndInvalidInputPreservesValidSample()
{
    SetForecastSettings(true, 0.5);
    TEST_ASSERT_TRUE(SubmitForecast({-4.0, 60, true, false}));
    TEST_ASSERT_TRUE(SubmitForecast({4.0, 60, true, true}));
    TEST_ASSERT_FALSE(SubmitForecast({NAN, 60, true, false}));
    TEST_ASSERT_FALSE(SubmitForecast({51.0, 60, true, false}));
    TEST_ASSERT_FALSE(SubmitForecast({2.5, 60, false, false}));
    TEST_ASSERT_FALSE(SubmitForecast({-4.0, 0, true, false}));
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 55.0);
    AssertNear(GetForecastState().TemperatureChange, 4.0);
}
void DirectCorrectionIsNotScaledTwiceAndDisableStopsIt()
{
    SetForecastSettings(true, 0.25);
    TEST_ASSERT_TRUE(SubmitForecastCorrection(1.5, 60));
    ProcessForecastCorrection();
    AssertNear(GetForecastState().Correction, 1.5);
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 58.5);
    SetForecastSettings(false, 0.25);
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 57.0);
}
void DelayedOlderRuntimeForecastCannotReplaceNewerPendingSample()
{
    SetForecastSettings(true, 0.5);
    const uint32_t receivedAt = millis();
    const uint32_t older = ReserveForecastSubmission();
    TEST_ASSERT_TRUE(SubmitForecast({4.0, 60, true, true}, receivedAt));
    // Equal millisecond timestamps must still preserve submission ordering.
    TEST_ASSERT_FALSE(SubmitForecast({-4.0, 60, true, false}, receivedAt, older));
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 55.0);
    TEST_ASSERT_TRUE(GetForecastState().RoomsSatisfied);
}

void DelayedOlderRuntimeForecastCannotReplaceAlreadyActiveNewerSample()
{
    SetForecastSettings(true, 0.5);
    const uint32_t older = ReserveForecastSubmission();
    ApplySample(-4.0, 60, false);
    TEST_ASSERT_FALSE(SubmitForecast({4.0, 60, true, true}, millis(), older));
    ProcessForecastCorrection();
    AssertNear(commandedValues.Heating.CalculatedFeedSetpoint, 59.0);
    TEST_ASSERT_FALSE(GetForecastState().RoomsSatisfied);
}
} // namespace

double CalculateFeedTemperature()
{
    const ForecastState state = GetForecastState();
    heatingInputs.ForecastCorrection = GetForecastCorrection(millis());
    heatingInputs.ForecastRoomsSatisfied = state.Enabled && state.RoomsSatisfied;
    return CalculateFeedTemperatureForInputs(heatingInputs).Setpoint;
}
void SetFeedTemperature()
{
    commandedValues.Heating.CalculatedFeedSetpoint = CalculateFeedTemperature();
}
void PublishHeatingTemperaturesAndStatus() {}

void setUp()
{
    heatingInputs = FeedTemperatureInputs{};
    heatingInputs.ValveScaling = true;
    heatingInputs.DynamicAdaption = true;
    heatingInputs.ValveOpening = 30.0;
    heatingInputs.OutsideTemperature = -10.0;
    SetForecastSettings(false, 0.5);
    SubmitForecastCorrection(0.0, 1);
    ProcessForecastCorrection();
    SetFeedTemperature();
}
void tearDown() {}
void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(AvailableSampleDoesNotHeatWhenDisabled);
    RUN_TEST(EnableAndCoefficientChangesRecalculateFreshSample);
    RUN_TEST(RoomPermissionChangesRecalculateWithoutCorrectionChange);
    RUN_TEST(ExpiryDropsCorrectionAndRoomPermission);
    RUN_TEST(LifetimeStartsAtReceiptNotApplication);
    RUN_TEST(NewestPendingSampleWinsAndInvalidInputPreservesValidSample);
    RUN_TEST(DirectCorrectionIsNotScaledTwiceAndDisableStopsIt);
    RUN_TEST(DelayedOlderRuntimeForecastCannotReplaceNewerPendingSample);
    RUN_TEST(DelayedOlderRuntimeForecastCannotReplaceAlreadyActiveNewerSample);
    UNITY_END();
}
void loop() {}
