#include <feed_temperature.h>
#include <Arduino.h>
#include <unity.h>
#include <math.h>

namespace
{
void AssertNear(double actual, double expected)
{
    TEST_ASSERT_TRUE(fabs(actual - expected) <= 0.001);
}

void AssertSetpoint(const FeedTemperatureInputs &inputs, double expected)
{
    const FeedTemperatureResult result = CalculateFeedTemperatureForInputs(inputs);
    AssertNear(result.Setpoint, expected);
}

void DisabledIsOffEvenWithInvalidCurveAndForecast()
{
    FeedTemperatureInputs inputs;
    inputs.Enabled = false;
    inputs.BasepointTemperature = inputs.EndpointTemperature;
    inputs.OutsideTemperature = 1000.0;
    inputs.ForecastCorrection = 2.0;

    const FeedTemperatureResult result = CalculateFeedTemperatureForInputs(inputs);
    AssertNear(result.Setpoint, 10.0);
}

void ZeroValveDemandIsOffBeforeAdaptationAndForecast()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.ValveOpening = 0.0;
    inputs.FeedAdaption = 18.0;
    inputs.ForecastCorrection = 2.0;

    const FeedTemperatureResult result = CalculateFeedTemperatureForInputs(inputs);
    AssertNear(result.Setpoint, 10.0);
}

void HybridOutdoorBaseAndValveDemandAt11Point5Degrees()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = 11.5;
    inputs.ValveOpening = 32.0;

    // WeatherBase=29.9193548 C; 32% of the remaining distance to
    // 75 C gives 44.3452 C, rounded to the nearest half degree.
    AssertSetpoint(inputs, 44.5);
}

void HybridSampleAt11Point59DegreesUsesWorkedHalfStep()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = 11.59;
    inputs.ValveOpening = 32.0;

    // The unrounded hybrid result is about 44.2168 C.
    AssertSetpoint(inputs, 44.0);
}

void HybridOutdoorBaseAndValveDemandAtZeroDegrees()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = 0.0;
    inputs.ValveOpening = 32.0;

    // The 60% weather cap gives a 49 C base; 32% demand gives 57.32 C.
    AssertSetpoint(inputs, 57.5);
}

void ColdWeatherAdaptionIsProportionalAndUnsaturated()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = 0.0;
    inputs.ValveOpening = 32.0;
    inputs.FeedAdaption = -3.0;

    // Weather and manual adaptation share the capped 0.6 fraction:
    // base=47.2 C; 32% demand gives 56.096 C.
    AssertSetpoint(inputs, 56.0);
}

void StaticValveScalingRoundsFractionWithoutIntegerTruncation()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.ValveOpening = 32.0;

    // 10 + (75 - 10) * 0.32 = 30.8 C.
    AssertSetpoint(inputs, 31.0);
}

void WeatherSetpointRisesAsOutsideTemperatureFalls()
{
    FeedTemperatureInputs inputs;
    inputs.DynamicAdaption = true;
    inputs.TargetAmbientTemperature = 20.0;
    inputs.FeedReturnTemperatureReference = 20.0;
    inputs.OutsideTemperature = 10.0;
    AssertSetpoint(inputs, 33.0);

    inputs.OutsideTemperature = 0.0;
    AssertSetpoint(inputs, 54.0);
}

void ForecastCorrectionAppliesPositiveAndNegativeTwoDegreeBounds()
{
    FeedTemperatureInputs inputs;
    inputs.OutsideTemperature = 10.0;
    inputs.ForecastCorrection = 20.0;
    AssertSetpoint(inputs, 35.0);

    inputs.ForecastCorrection = -20.0;
    AssertSetpoint(inputs, 31.0);

    inputs.OutsideTemperature = inputs.EndpointTemperature;
    AssertSetpoint(inputs, 10.0);

    inputs.OutsideTemperature = inputs.BasepointTemperature;
    inputs.FeedMaximum = 62.3;
    inputs.ForecastCorrection = 20.0;
    const FeedTemperatureResult result = CalculateFeedTemperatureForInputs(inputs);
    TEST_ASSERT_TRUE(result.Setpoint <= inputs.FeedMaximum);
}

void NegativeForecastCannotReduceHybridDemandWithUnknownRoomDeficits()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = 11.59;
    inputs.ValveOpening = 32.0;
    inputs.ForecastCorrection = -2.0;

    // A diluted aggregate can hide a fully open, under-target room.
    // Without individual room information, retain the 44 C heating request.
    AssertSetpoint(inputs, 44.0);
}

void NegativeForecastCannotReduceStaticValveDemand()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.ValveOpening = 32.0;
    inputs.ForecastCorrection = -2.0;

    AssertSetpoint(inputs, 31.0);
}

void PositiveForecastStillSupportsHybridValveDemand()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = 11.59;
    inputs.ValveOpening = 32.0;
    inputs.ForecastCorrection = 2.0;

    AssertSetpoint(inputs, 46.0);
}

void ForecastCorrectionIsIgnoredForOverrideBoostAndOff()
{
    FeedTemperatureInputs inputs;
    inputs.OverrideSetpoint = true;
    inputs.FeedSetpoint = 42.0;
    inputs.ForecastCorrection = 2.0;
    AssertSetpoint(inputs, 42.0);

    inputs.OverrideSetpoint = false;
    inputs.Boost = true;
    AssertSetpoint(inputs, 75.0);

    inputs.Boost = false;
    inputs.Enabled = false;
    AssertSetpoint(inputs, 10.0);
}

void NegativeAdaptationAndSaturationStayInRange()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.ValveOpening = 50.0;
    inputs.FeedAdaption = -30.0;
    AssertSetpoint(inputs, 27.5);

    inputs.FeedAdaption = 100.0;
    inputs.ValveOpening = 100.0;
    AssertSetpoint(inputs, 75.0);
}

void AllOperatingModesRespectReportedNonHalfStepMaximum()
{
    FeedTemperatureInputs inputs;
    inputs.FeedMaximum = 62.3;
    inputs.OverrideSetpoint = true;
    inputs.FeedSetpoint = 90.0;
    TEST_ASSERT_TRUE(CalculateFeedTemperatureForInputs(inputs).Setpoint <= inputs.FeedMaximum);

    inputs.OverrideSetpoint = false;
    inputs.Boost = true;
    TEST_ASSERT_TRUE(CalculateFeedTemperatureForInputs(inputs).Setpoint <= inputs.FeedMaximum);

    inputs.Boost = false;
    inputs.FastHeatup = true;
    inputs.AmbientTemperature = 10.0;
    inputs.TargetAmbientTemperature = 21.0;
    inputs.ReferenceAmbientTemperature = 17.0;
    TEST_ASSERT_TRUE(CalculateFeedTemperatureForInputs(inputs).Setpoint <= inputs.FeedMaximum);

    inputs.FastHeatup = false;
    inputs.OutsideTemperature = inputs.BasepointTemperature;
    TEST_ASSERT_TRUE(CalculateFeedTemperatureForInputs(inputs).Setpoint <= inputs.FeedMaximum);

    inputs.ValveScaling = true;
    inputs.ValveOpening = inputs.MaxValveOpening;
    TEST_ASSERT_TRUE(CalculateFeedTemperatureForInputs(inputs).Setpoint <= inputs.FeedMaximum);
}

void ZeroMinimumPreservesColdWeatherCurve()
{
    FeedTemperatureInputs inputs;
    inputs.MinimumFeedTemperature = 0.0;
    inputs.OutsideTemperature = -10.0;
    AssertSetpoint(inputs, 75.0);

    inputs.OutsideTemperature = 10.0;
    AssertSetpoint(inputs, 26.5);

    inputs.OutsideTemperature = inputs.EndpointTemperature;
    AssertSetpoint(inputs, 10.0);
}

void ZeroMinimumStaticValveFullDemandReachesMaximum()
{
    FeedTemperatureInputs inputs;
    inputs.MinimumFeedTemperature = 0.0;
    inputs.ValveScaling = true;
    inputs.ValveOpening = inputs.MaxValveOpening;
    AssertSetpoint(inputs, 75.0);
}

void MinimumAboveReducedMaximumIsLimitedRatherThanOff()
{
    FeedTemperatureInputs inputs;
    inputs.MinimumFeedTemperature = 40.0;
    inputs.FeedMaximum = 35.0;
    AssertSetpoint(inputs, 35.0);

    inputs.ValveScaling = true;
    inputs.ValveOpening = 32.0;
    AssertSetpoint(inputs, 35.0);

    inputs.DynamicAdaption = true;
    AssertSetpoint(inputs, 35.0);

    inputs.FeedMaximum = 35.3;
    AssertSetpoint(inputs, 35.0);

    inputs.DynamicAdaption = false;
    AssertSetpoint(inputs, 35.0);

    inputs.ValveScaling = false;
    AssertSetpoint(inputs, 35.0);
}

void NonFiniteMinimumStillFailsOff()
{
    FeedTemperatureInputs inputs;
    inputs.OutsideTemperature = -10.0;
    inputs.MinimumFeedTemperature = NAN;
    AssertSetpoint(inputs, 10.0);

    inputs.MinimumFeedTemperature = INFINITY;
    AssertSetpoint(inputs, 10.0);

    inputs.MinimumFeedTemperature = -INFINITY;
    AssertSetpoint(inputs, 10.0);
}

void InvalidRequiredInputsAndDegenerateCurveFailOff()
{
    FeedTemperatureInputs inputs;
    inputs.OutsideTemperatureValid = false;
    AssertNear(CalculateFeedTemperatureForInputs(inputs).Setpoint, 10.0);

    inputs = FeedTemperatureInputs{};
    inputs.BasepointTemperature = inputs.EndpointTemperature;
    AssertNear(CalculateFeedTemperatureForInputs(inputs).Setpoint, 10.0);

    inputs = FeedTemperatureInputs{};
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.MaxValveOpening = 0.0;
    inputs.ValveOpening = 20.0;
    AssertNear(CalculateFeedTemperatureForInputs(inputs).Setpoint, 10.0);

    inputs = FeedTemperatureInputs{};
    inputs.OutsideTemperature = NAN;
    AssertNear(CalculateFeedTemperatureForInputs(inputs).Setpoint, 10.0);

    inputs = FeedTemperatureInputs{};
    inputs.MinimumFeedTemperature = NAN;
    AssertNear(CalculateFeedTemperatureForInputs(inputs).Setpoint, 10.0);

    inputs = FeedTemperatureInputs{};
    inputs.FeedMaximum = NAN;
    AssertNear(CalculateFeedTemperatureForInputs(inputs).Setpoint, 10.0);
}

void FailSafeHoldReturnsMinimumDespiteInvalidWeatherCurve()
{
    FeedTemperatureInputs inputs;
    inputs.HoldMinimumTemperature = true;
    inputs.MinimumFeedTemperature = 35.0;
    inputs.BasepointTemperature = inputs.EndpointTemperature;
    inputs.OutsideTemperatureValid = false;
    inputs.FeedAdaption = NAN;
    inputs.ForecastCorrection = 2.0;
    AssertSetpoint(inputs, 35.0);

    inputs.OverrideSetpoint = true;
    inputs.FeedSetpoint = 42.0;
    AssertSetpoint(inputs, 42.0);

    inputs.OverrideSetpoint = false;
    inputs.Boost = true;
    AssertSetpoint(inputs, 75.0);

    inputs.Enabled = false;
    AssertSetpoint(inputs, 10.0);

    inputs = FeedTemperatureInputs{};
    inputs.HoldMinimumTemperature = true;
    inputs.MinimumFeedTemperature = 35.0;
    inputs.ValveScaling = true;
    inputs.ValveOpening = 0.0;
    AssertSetpoint(inputs, 10.0);
}

void FinalOutputLimiterFloorsActualMaximumAndPreservesOff()
{
    AssertNear(LimitFeedTemperatureOutput(55.3, 55.3), 55.0);
    AssertNear(LimitFeedTemperatureOutput(20.0, 75.0, 35.0), 35.0);
    AssertNear(LimitFeedTemperatureOutput(10.0, 75.0), 10.0);
    AssertNear(LimitFeedTemperatureOutput(NAN, 75.0), 10.0);
    AssertNear(LimitFeedTemperatureOutput(55.0, NAN), 10.0);
}

void WeatherSupportCapPreservesMildWeatherAndValveAuthorityInWinter()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.ValveOpening = 30.0;
    inputs.OutsideTemperature = 12.42;
    AssertSetpoint(inputs, 42.0);

    inputs.OutsideTemperature = 2.4;
    AssertSetpoint(inputs, 57.0);
    inputs.OutsideTemperature = -10.0;
    AssertSetpoint(inputs, 57.0);
    inputs.OutsideTemperature = -30.0;
    AssertSetpoint(inputs, 57.0);
    inputs.ValveOpening = 1.0;
    AssertSetpoint(inputs, 49.5);
    inputs.ValveOpening = 100.0;
    AssertSetpoint(inputs, 75.0);
    inputs.ValveOpening = 0.0;
    AssertSetpoint(inputs, 10.0);
}

void WeatherSupportExtremesRestoreFullCurveOrRemoveWeatherContribution()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = -10.0;
    inputs.ValveOpening = 30.0;
    inputs.WeatherSupportMaximum = 100.0;
    AssertSetpoint(inputs, 75.0);

    inputs.WeatherSupportMaximum = 0.0;
    inputs.FeedAdaption = 18.0;
    AssertSetpoint(inputs, 29.5);
}

void WeatherSupportCapDoesNotChangeOtherModesOrPhysicalCeiling()
{
    FeedTemperatureInputs inputs;
    inputs.WeatherSupportMaximum = 0.0;
    inputs.OutsideTemperature = -10.0;
    AssertSetpoint(inputs, 75.0);

    inputs.ValveScaling = true;
    inputs.ValveOpening = 30.0;
    AssertSetpoint(inputs, 29.5);
    inputs.Boost = true;
    AssertSetpoint(inputs, 75.0);
    inputs.Boost = false;
    inputs.OverrideSetpoint = true;
    inputs.FeedSetpoint = 42.0;
    AssertSetpoint(inputs, 42.0);

    inputs.OverrideSetpoint = false;
    inputs.DynamicAdaption = true;
    inputs.WeatherSupportMaximum = 60.0;
    inputs.FeedMaximum = 62.3;
    inputs.ValveOpening = 100.0;
    inputs.ForecastCorrection = 2.0;
    AssertSetpoint(inputs, 62.0);
}

void InvalidPureWeatherSupportCapUsesSafeDefault()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = -10.0;
    inputs.ValveOpening = 30.0;
    inputs.WeatherSupportMaximum = NAN;
    AssertSetpoint(inputs, 57.0);
    inputs.WeatherSupportMaximum = -1.0;
    AssertSetpoint(inputs, 57.0);
    inputs.WeatherSupportMaximum = 101.0;
    AssertSetpoint(inputs, 57.0);
}

void ForecastTrendCoefficientUsesBothSignsAndBounds()
{
    AssertNear(DeriveForecastCorrection(-2.0, 0.5), 1.0);
    AssertNear(DeriveForecastCorrection(2.0, 0.5), -1.0);
    AssertNear(DeriveForecastCorrection(-10.0, 0.5), 2.0);
    AssertNear(DeriveForecastCorrection(10.0, 0.5), -2.0);
    AssertNear(DeriveForecastCorrection(-4.0, 0.0), 0.0);
    AssertNear(DeriveForecastCorrection(-1.0, 1.0), 1.0);
    AssertNear(DeriveForecastCorrection(NAN, 0.5), 0.0);
    AssertNear(DeriveForecastCorrection(51.0, 0.5), 0.0);
    AssertNear(DeriveForecastCorrection(-4.0, NAN), 0.0);
    AssertNear(DeriveForecastCorrection(-4.0, 1.1), 0.0);
}

void WarmingForecastRequiresSatisfiedRoomsInBothValveModes()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = -10.0;
    inputs.ValveOpening = 30.0;
    inputs.ForecastCorrection = DeriveForecastCorrection(4.0, 0.5);
    AssertSetpoint(inputs, 57.0);
    inputs.ForecastRoomsSatisfied = true;
    AssertSetpoint(inputs, 55.0);

    inputs.DynamicAdaption = false;
    AssertSetpoint(inputs, 27.5);
    inputs.ForecastRoomsSatisfied = false;
    AssertSetpoint(inputs, 29.5);
}

void ForecastCoolingDoesNotNeedSatisfiedRoomsOrCreateDemand()
{
    FeedTemperatureInputs inputs;
    inputs.ValveScaling = true;
    inputs.DynamicAdaption = true;
    inputs.OutsideTemperature = -10.0;
    inputs.ValveOpening = 30.0;
    inputs.ForecastCorrection = DeriveForecastCorrection(-4.0, 0.5);
    AssertSetpoint(inputs, 59.0);
    inputs.ValveOpening = 0.0;
    AssertSetpoint(inputs, 10.0);
    inputs.ValveOpening = 30.0;
    inputs.Enabled = false;
    AssertSetpoint(inputs, 10.0);
}

void FastHeatupCompletionRequestsDisableWithoutChangingNormalSetpoint()
{
    FeedTemperatureInputs inputs;
    inputs.FastHeatup = true;
    inputs.AmbientTemperature = inputs.TargetAmbientTemperature;

    const FeedTemperatureResult result = CalculateFeedTemperatureForInputs(inputs);
    TEST_ASSERT_TRUE(result.DisableFastHeatup);
    AssertNear(result.Setpoint, 33.0);
    inputs.AmbientTemperature = inputs.TargetAmbientTemperature - 1.0;
    const FeedTemperatureResult incomplete = CalculateFeedTemperatureForInputs(inputs);
    TEST_ASSERT_FALSE(incomplete.DisableFastHeatup);
}
} // namespace

void setup()
{
    // Give the host test monitor time to attach after the upload resets the ESP32.
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(DisabledIsOffEvenWithInvalidCurveAndForecast);
    RUN_TEST(ZeroValveDemandIsOffBeforeAdaptationAndForecast);
    RUN_TEST(HybridOutdoorBaseAndValveDemandAt11Point5Degrees);
    RUN_TEST(HybridOutdoorBaseAndValveDemandAtZeroDegrees);
    RUN_TEST(ColdWeatherAdaptionIsProportionalAndUnsaturated);
    RUN_TEST(HybridSampleAt11Point59DegreesUsesWorkedHalfStep);
    RUN_TEST(StaticValveScalingRoundsFractionWithoutIntegerTruncation);
    RUN_TEST(ForecastCorrectionAppliesPositiveAndNegativeTwoDegreeBounds);
    RUN_TEST(ForecastCorrectionIsIgnoredForOverrideBoostAndOff);
    RUN_TEST(NegativeForecastCannotReduceHybridDemandWithUnknownRoomDeficits);
    RUN_TEST(NegativeForecastCannotReduceStaticValveDemand);
    RUN_TEST(PositiveForecastStillSupportsHybridValveDemand);
    RUN_TEST(NegativeAdaptationAndSaturationStayInRange);
    RUN_TEST(AllOperatingModesRespectReportedNonHalfStepMaximum);
    RUN_TEST(ZeroMinimumPreservesColdWeatherCurve);
    RUN_TEST(ZeroMinimumStaticValveFullDemandReachesMaximum);
    RUN_TEST(MinimumAboveReducedMaximumIsLimitedRatherThanOff);
    RUN_TEST(NonFiniteMinimumStillFailsOff);
    RUN_TEST(InvalidRequiredInputsAndDegenerateCurveFailOff);
    RUN_TEST(FailSafeHoldReturnsMinimumDespiteInvalidWeatherCurve);
    RUN_TEST(FinalOutputLimiterFloorsActualMaximumAndPreservesOff);
    RUN_TEST(FastHeatupCompletionRequestsDisableWithoutChangingNormalSetpoint);
    RUN_TEST(WeatherSetpointRisesAsOutsideTemperatureFalls);
    RUN_TEST(WeatherSupportCapPreservesMildWeatherAndValveAuthorityInWinter);
    RUN_TEST(WeatherSupportExtremesRestoreFullCurveOrRemoveWeatherContribution);
    RUN_TEST(WeatherSupportCapDoesNotChangeOtherModesOrPhysicalCeiling);
    RUN_TEST(InvalidPureWeatherSupportCapUsesSafeDefault);
    RUN_TEST(ForecastTrendCoefficientUsesBothSignsAndBounds);
    RUN_TEST(WarmingForecastRequiresSatisfiedRoomsInBothValveModes);
    RUN_TEST(ForecastCoolingDoesNotNeedSatisfiedRoomsOrCreateDemand);
    UNITY_END();
}

void loop() {}
