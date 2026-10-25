#include <feed_temperature.h>
#include <cmath>

namespace
{
constexpr double offTemperature = 10.0;

double clamp(double value, double minimum, double maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

FeedTemperatureResult finish(double value, double maximum, bool disableFastHeatup)
{
    return {LimitFeedTemperatureOutput(value, maximum), disableFastHeatup};
}
} // namespace

double LimitFeedTemperatureOutput(double requested, double maximum, double minimum)
{
    if (!std::isfinite(requested) || requested <= offTemperature ||
        !std::isfinite(maximum) || maximum < offTemperature ||
        !std::isfinite(maximum * 2.0) || !std::isfinite(minimum))
        return offTemperature;
    const double halfStepMaximum = std::floor(maximum * 2.0) / 2.0;
    const double boundedMinimum = clamp(minimum, offTemperature, halfStepMaximum);
    const double value = clamp(requested, boundedMinimum, halfStepMaximum);
    return clamp(std::round(value * 2.0) / 2.0, offTemperature, halfStepMaximum);
}

double DeriveForecastCorrection(double temperatureChange, double coefficient)
{
    if (!std::isfinite(temperatureChange) || temperatureChange < -50.0 || temperatureChange > 50.0 ||
        !std::isfinite(coefficient) || coefficient < 0.0 || coefficient > 1.0)
        return 0.0;
    return clamp(-coefficient * temperatureChange, -2.0, 2.0);
}

FeedTemperatureResult CalculateFeedTemperatureForInputs(const FeedTemperatureInputs &inputs)
{
    const FeedTemperatureResult off = {offTemperature, false};
    if (!inputs.Enabled || !std::isfinite(inputs.FeedMaximum) || inputs.FeedMaximum < offTemperature ||
        !std::isfinite(inputs.FeedMaximum * 2.0))
        return off;

    if (inputs.ValveScaling && (!std::isfinite(inputs.ValveOpening) || inputs.ValveOpening <= 0.0 ||
                               !std::isfinite(inputs.MaxValveOpening) || inputs.MaxValveOpening <= 0.0))
        return {offTemperature, inputs.FastHeatup};

    if (inputs.OverrideSetpoint)
        return finish(inputs.FeedSetpoint, inputs.FeedMaximum, false);
    if (inputs.Boost)
        return finish(inputs.FeedMaximum, inputs.FeedMaximum, false);

    if (inputs.HoldMinimumTemperature)
        return finish(inputs.MinimumFeedTemperature, inputs.FeedMaximum, false);

    if (!std::isfinite(inputs.MinimumFeedTemperature) || !std::isfinite(inputs.FeedAdaption))
        return off;


    const bool disableFastHeatup = inputs.ValveScaling && inputs.FastHeatup;
    double weatherBase = 0.0;
    if (!inputs.ValveScaling || inputs.DynamicAdaption)
    {
        const double range = inputs.EndpointTemperature - inputs.BasepointTemperature;
        if (!inputs.OutsideTemperatureValid || !std::isfinite(inputs.OutsideTemperature) ||
            !std::isfinite(inputs.EndpointTemperature) || !std::isfinite(inputs.BasepointTemperature) ||
            !std::isfinite(range) || range <= 0.0)
            return {offTemperature, disableFastHeatup};
        double fraction = clamp((inputs.EndpointTemperature - inputs.OutsideTemperature) / range, 0.0, 1.0);
        if (inputs.ValveScaling)
        {
            const double maximum = std::isfinite(inputs.WeatherSupportMaximum) &&
                                   inputs.WeatherSupportMaximum >= 0.0 && inputs.WeatherSupportMaximum <= 100.0
                                       ? inputs.WeatherSupportMaximum : 60.0;
            fraction = clamp(fraction, 0.0, maximum / 100.0);
        }
        weatherBase = inputs.MinimumFeedTemperature + (inputs.FeedMaximum - inputs.MinimumFeedTemperature) * fraction;
        if (inputs.ValveScaling)
            weatherBase += inputs.FeedAdaption * fraction;
    }

    double temperature;
    if (inputs.ValveScaling)
    {
        const double demand = clamp(inputs.ValveOpening / inputs.MaxValveOpening, 0.0, 1.0);
        const double base = inputs.DynamicAdaption
                                ? clamp(weatherBase, offTemperature, inputs.FeedMaximum)
                                : inputs.MinimumFeedTemperature + inputs.FeedAdaption;
        temperature = base + (inputs.FeedMaximum - base) * demand;
    }
    else
    {
        temperature = weatherBase + inputs.FeedAdaption;
        if (inputs.DynamicAdaption && !inputs.FastHeatup)
        {
            if (!std::isfinite(inputs.TargetAmbientTemperature) || !std::isfinite(inputs.FeedReturnTemperatureReference))
                return off;
            temperature += inputs.TargetAmbientTemperature - inputs.FeedReturnTemperatureReference;
        }
        if (inputs.FastHeatup)
        {
            if (!std::isfinite(inputs.AmbientTemperature) || !std::isfinite(inputs.TargetAmbientTemperature))
                return off;
            if (inputs.AmbientTemperature >= inputs.TargetAmbientTemperature)
            {
                if (temperature > offTemperature && std::isfinite(inputs.ForecastCorrection))
                    temperature += clamp(inputs.ForecastCorrection, -2.0, 2.0);
                return finish(temperature, inputs.FeedMaximum, true);
            }
            const double initialDifference = inputs.TargetAmbientTemperature - inputs.ReferenceAmbientTemperature;
            if (!std::isfinite(initialDifference) || initialDifference <= 0.0)
                return off;
            const double fraction = clamp((inputs.TargetAmbientTemperature - inputs.AmbientTemperature) / initialDifference, 0.0, 1.0);
            temperature += (inputs.FeedMaximum - temperature) * fraction;
        }
    }

    // A negative valve correction requires explicit permission from every room.
    if (temperature > offTemperature && std::isfinite(inputs.ForecastCorrection) &&
        (!inputs.ValveScaling || inputs.ForecastCorrection >= 0.0 || inputs.ForecastRoomsSatisfied))
        temperature += clamp(inputs.ForecastCorrection, -2.0, 2.0);
    return finish(temperature, inputs.FeedMaximum, disableFastHeatup);
}
