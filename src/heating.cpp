#include <Arduino.h>
#include <heating.h>
#include <mqtt.h>
#include <telnet.h>
#include <feed_temperature.h>
#include <forecast.h>
#include <failsafe.h>
#include <cmath>

CeraValues ceraValues;

namespace
{
constexpr double manufacturerHeatingOffSetpointC = 10.0;
constexpr double setpointComparisonTolerance = 0.01;
unsigned long lowSetpointSince = 0;
bool lowSetpointPending = false;
bool setpointShutdownActive = false;

void activateSetpointShutdown()
{
    if (!setpointShutdownActive)
        Log.printf("Feed setpoint remained at %.1f C for %lu seconds. Switching heating off.\r\n",
                   manufacturerHeatingOffSetpointC,
                   configuration.General.SetpointOffDelaySeconds);
    setpointShutdownActive = true;
}
} // namespace

// Applies the manufacturer's convention that a 10 C feed setpoint means off.
// The latch prevents repeated Enabled=true traffic from restarting the heating
// until a genuinely higher effective setpoint is requested.
bool EnforceHeatingSetpointShutdown(double effectiveSetpoint)
{
    if (effectiveSetpoint > manufacturerHeatingOffSetpointC + setpointComparisonTolerance)
    {
        lowSetpointPending = false;
        setpointShutdownActive = false;
        return false;
    }

    if (!lowSetpointPending)
    {
        lowSetpointPending = true;
        lowSetpointSince = millis();
    }

    if (!setpointShutdownActive &&
        millis() - lowSetpointSince >= configuration.General.SetpointOffDelaySeconds * 1000UL)
        activateSetpointShutdown();

    if (!setpointShutdownActive || !commandedValues.Heating.Active)
        return false;

    commandedValues.Heating.Active = false;
    return true;
}

void UpdateHeatingSetpointShutdown()
{
    if (!lowSetpointPending || setpointShutdownActive ||
        millis() - lowSetpointSince < configuration.General.SetpointOffDelaySeconds * 1000UL)
        return;

    activateSetpointShutdown();
    if (!commandedValues.Heating.Active)
        return;

    commandedValues.Heating.Active = false;
    SetFeedTemperature();
    PublishHeatingTemperaturesAndStatus();
}


double CalculateFeedTemperature()
{
    FeedTemperatureInputs inputs;
    inputs.Enabled = commandedValues.Heating.Active;
    inputs.OverrideSetpoint = commandedValues.Heating.OverrideSetpoint;
    inputs.Boost = commandedValues.Heating.Boost;
    inputs.ValveScaling = commandedValues.Heating.ValveScaling;
    inputs.DynamicAdaption = commandedValues.Heating.DynamicAdaption;
    inputs.FastHeatup = commandedValues.Heating.FastHeatup;
    inputs.OutsideTemperatureValid = configuration.Features.UseAuxiliaryOutsideTempReference || ceraValues.General.HasReceivedOT;
    inputs.OutsideTemperature = configuration.Features.UseAuxiliaryOutsideTempReference
                                    ? commandedValues.Heating.AuxiliaryTemperature
                                    : ceraValues.General.OutsideTemperature;
    inputs.FeedMaximum = ceraValues.Heating.HasReceivedFeedMaximum ? ceraValues.Heating.FeedMaximum : NAN;
    inputs.FeedSetpoint = commandedValues.Heating.FeedSetpoint;
    inputs.MinimumFeedTemperature = commandedValues.Heating.MinimumFeedTemperature;
    inputs.BasepointTemperature = commandedValues.Heating.BasepointTemperature;
    inputs.EndpointTemperature = commandedValues.Heating.EndpointTemperature;
    inputs.HoldMinimumTemperature = IsFailSafeActive() &&
                                    inputs.BasepointTemperature == inputs.MinimumFeedTemperature &&
                                    inputs.EndpointTemperature == inputs.MinimumFeedTemperature;
    inputs.FeedAdaption = commandedValues.Heating.FeedAdaption;
    inputs.WeatherSupportMaximum = GetWeatherSupportMaximum();
    inputs.ValveOpening = commandedValues.Heating.ValveOpening;
    inputs.MaxValveOpening = commandedValues.Heating.MaxValveOpening;
    inputs.AmbientTemperature = commandedValues.Heating.AmbientTemperature;
    inputs.TargetAmbientTemperature = commandedValues.Heating.TargetAmbientTemperature;
    inputs.ReferenceAmbientTemperature = commandedValues.Heating.ReferenceAmbientTemperature;
    inputs.FeedReturnTemperatureReference = ceraValues.Auxiliary.FeedReturnTemperatureReference;
    const ForecastState forecast = GetForecastState();
    inputs.ForecastCorrection = IsFailSafeActive() || !forecast.Enabled ? 0.0 : forecast.Correction;
    inputs.ForecastRoomsSatisfied = forecast.Enabled && forecast.RoomsSatisfied;
    const FeedTemperatureResult result = CalculateFeedTemperatureForInputs(inputs);
    if (result.DisableFastHeatup)
        commandedValues.Heating.FastHeatup = false;
    return result.Setpoint;
}

// Converts the value to its half-step representation (= value times two)
int ConvertFeedTemperature(double temperature)
{
    return temperature * 2;
}
