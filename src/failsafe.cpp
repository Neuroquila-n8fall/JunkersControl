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
unsigned long mqttDisconnectedSince = 0;
CommandedValues remoteValuesBeforeFailSafe;
bool hasSavedRemoteValues = false;

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

void enterFailSafe(const char *reason)
{
    if (!failSafeActive)
    {
        remoteValuesBeforeFailSafe = commandedValues;
        hasSavedRemoteValues = true;
    }

    failSafeActive = true;
    clearRemoteOverrides();
    Log.println(reason);
}

void leaveFailSafe(const char *reason)
{
    if (!failSafeActive)
        return;

    if (hasSavedRemoteValues)
        commandedValues = remoteValuesBeforeFailSafe;

    hasSavedRemoteValues = false;
    failSafeActive = false;
    SetFeedTemperature();
    Log.println(reason);
}
} // namespace

void SetupFailSafe()
{
    mqttDisconnectedSince = millis();
    failSafeActive = configuration.FailSafe.Enabled;
    if (failSafeActive)
    {
        remoteValuesBeforeFailSafe = commandedValues;
        hasSavedRemoteValues = true;
        clearRemoteOverrides();
        Log.println("Starting in fail-safe mode until MQTT is connected.");
    }
}

void NotifyValidHeatingCommand()
{
    // Kept as the common command hook. Fail-safe ownership is intentionally
    // based on MQTT connectivity, not on traffic frequency or command source.
}

void UpdateFailSafe()
{
    if (!configuration.FailSafe.Enabled)
    {
        leaveFailSafe("Fail-safe disabled. Restoring runtime control values.");
        mqttDisconnectedSince = 0;
        return;
    }

    if (client.connected())
    {
        mqttDisconnectedSince = 0;
        leaveFailSafe("MQTT connected. Leaving fail-safe mode and restoring runtime control values.");
        return;
    }

    const unsigned long now = millis();
    if (mqttDisconnectedSince == 0)
        mqttDisconnectedSince = now;

    const unsigned long timeoutMs = configuration.FailSafe.CommandTimeoutSeconds * 1000UL;
    if (!failSafeActive && now - mqttDisconnectedSince >= timeoutMs)
        enterFailSafe("MQTT offline timeout expired. Entering fail-safe mode.");

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
