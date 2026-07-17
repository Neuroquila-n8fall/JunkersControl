#include <failsafe.h>

#include <Arduino.h>
#include <configuration.h>
#include <heating.h>
#include <mqtt.h>
#include <telnet.h>
#include <timesync.h>

namespace
{
bool failSafeActive = false;
unsigned long lastValidHeatingCommand = 0;

bool isInsideSchedule(int currentMinute, int startMinute, int stopMinute)
{
    if (startMinute == stopMinute)
        return true;
    if (startMinute < stopMinute)
        return currentMinute >= startMinute && currentMinute < stopMinute;
    return currentMinute >= startMinute || currentMinute < stopMinute;
}

bool scheduledHeatingState(bool &hasValidClock)
{
    int hour = 0;
    int minute = 0;

    if (ceraValues.Time.HasValidTime && millis() - ceraValues.Time.LastUpdateMillis <= 300000UL)
    {
        hasValidClock = true;
        hour = ceraValues.Time.Hours;
        minute = ceraValues.Time.Minutes;
    }
    else if (TimeIsSynced())
    {
        hasValidClock = true;
        hour = myTZ.hour();
        minute = myTZ.minute();
    }
    else
    {
        hasValidClock = false;
        return configuration.FailSafe.HeatWhenTimeUnknown;
    }

    const int currentMinute = hour * 60 + minute;
    const int startMinute = configuration.FailSafe.StartHour * 60 + configuration.FailSafe.StartMinute;
    const int stopMinute = configuration.FailSafe.StopHour * 60 + configuration.FailSafe.StopMinute;
    return isInsideSchedule(currentMinute, startMinute, stopMinute);
}

void clearRemoteOverrides()
{
    commandedValues.Heating.Boost = false;
    commandedValues.Heating.BoostTimeCountdown = 0;
    commandedValues.Heating.FastHeatup = false;
    commandedValues.Heating.OverrideSetpoint = false;
    commandedValues.Heating.ValveScaling = false;
    commandedValues.Heating.DynamicAdaption = false;
    commandedValues.Heating.FeedAdaption = 0;
}
} // namespace

void SetupFailSafe()
{
    lastValidHeatingCommand = millis();
    failSafeActive = configuration.FailSafe.Enabled;
    if (failSafeActive)
    {
        clearRemoteOverrides();
        Log.println("Starting in fail-safe mode until a fresh heating command is received.");
    }
}

void NotifyValidHeatingCommand()
{
    lastValidHeatingCommand = millis();
    if (failSafeActive)
    {
        failSafeActive = false;
        Log.println("Fresh heating command received. Leaving fail-safe mode.");
    }
}

void UpdateFailSafe()
{
    if (!configuration.FailSafe.Enabled)
    {
        failSafeActive = false;
        return;
    }

    const unsigned long timeoutMs = configuration.FailSafe.CommandTimeoutSeconds * 1000UL;
    if (!failSafeActive && millis() - lastValidHeatingCommand >= timeoutMs)
    {
        failSafeActive = true;
        clearRemoteOverrides();
        Log.println("Heating command lease expired. Entering fail-safe mode.");
    }

    if (!failSafeActive)
        return;

    // Reassert the safe profile in case another local path changed a command.
    clearRemoteOverrides();
    bool hasValidClock = false;
    commandedValues.Heating.Active = scheduledHeatingState(hasValidClock);
    if (hasValidClock)
    {
        commandedValues.Heating.BasepointTemperature = configuration.FailSafe.BasepointTemperature;
        commandedValues.Heating.EndpointTemperature = configuration.FailSafe.EndpointTemperature;
    }
    else
    {
        // With no trustworthy time, never run a normal heating curve. If the
        // configured policy requests heat, hold the minimum feed temperature.
        commandedValues.Heating.BasepointTemperature = configuration.FailSafe.MinimumFeedTemperature;
        commandedValues.Heating.EndpointTemperature = configuration.FailSafe.MinimumFeedTemperature;
    }
    commandedValues.Heating.MinimumFeedTemperature = configuration.FailSafe.MinimumFeedTemperature;
}

bool IsFailSafeActive()
{
    return failSafeActive;
}

double ApplyFailSafeFeedLimits(double temperature)
{
    if (!failSafeActive)
        return temperature;

    return constrain(temperature,
                     configuration.FailSafe.MinimumFeedTemperature,
                     configuration.FailSafe.MaximumFeedTemperature);
}
