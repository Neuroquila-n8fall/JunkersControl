#ifndef FEED_TEMPERATURE_H
#define FEED_TEMPERATURE_H

struct FeedTemperatureInputs
{
    bool Enabled = true;
    bool OverrideSetpoint = false;
    bool Boost = false;
    bool ValveScaling = false;
    bool DynamicAdaption = false;
    bool FastHeatup = false;
    bool OutsideTemperatureValid = true;
    // Intentional no-clock fail-safe hold, not an ordinary degenerate curve.
    bool HoldMinimumTemperature = false;
    double FeedSetpoint = 40.0;
    double FeedMaximum = 75.0;
    double MinimumFeedTemperature = 10.0;
    double BasepointTemperature = -10.0;
    double EndpointTemperature = 21.0;
    double OutsideTemperature = 10.0;
    double FeedAdaption = 0.0;
    double WeatherSupportMaximum = 60.0;
    double ValveOpening = 0.0;
    double MaxValveOpening = 100.0;
    double AmbientTemperature = 20.0;
    double TargetAmbientTemperature = 21.0;
    double ReferenceAmbientTemperature = 17.0;
    double FeedReturnTemperatureReference = 20.0;
    double ForecastCorrection = 0.0;
    bool ForecastRoomsSatisfied = false;
};

struct FeedTemperatureResult
{
    double Setpoint;
    bool DisableFastHeatup;
};

// Shared by production firmware and temperature input/output test firmware.
// No CAN, network, clock, heap allocation or mutable global dependencies.
FeedTemperatureResult CalculateFeedTemperatureForInputs(const FeedTemperatureInputs &inputs);

// Shared final wire-output limiter; preserves off and floors the safety ceiling.
double LimitFeedTemperatureOutput(double requested, double maximum, double minimum = 10.0);

// Expected outside-temperature change in C; coefficient is C feed per C outside.
double DeriveForecastCorrection(double temperatureChange, double coefficient);

#endif
