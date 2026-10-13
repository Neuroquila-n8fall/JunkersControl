# MQTT Command Messages

These examples are JSON messages accepted by Cerasmarter's configured MQTT command topics.

## Heating Parameters

- [Heating Parameters](ExampleHeatingParameters.json) lets you configure the temperatures, behavior of your heating circuit and operation mode

```json
{
    "Enabled": false,
    "FeedSetpoint": 10,
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
```

###### Enable
```json
    "Enabled": false,
```

This setting instructs the controller to enable or disable heating operation.

###### Feed Setpoint

```json
    "FeedSetpoint": 10,
```

The requested feed temperature setpoint when ...
```json
    "OverrideSetpoint": true
```

override is enabled.

###### Base Setpoint (Weather Guided)

```json
    "FeedBaseSetpoint": -10,
```

At or below this outside-temperature basepoint, the outside curve reaches the physical maximum feed temperature reported by the boiler. This is a temperature request, not a measurement of burner power.

###### Cutoff Temperature (Weather Guided)

```json
    "FeedCutOff": 22,
```

At this outside temperature the outside curve reaches its minimum. In dynamic valve mode, an actual valve demand can still raise the request above that baseline; zero valve demand remains off.

###### Anti-Freeze

```json
    "FeedMinimum": 10,
```

The manufacturer-defined 10 °C feed request means heating off; it is not proof of guaranteed frost protection. Every effective heating feed request is bounded by that floor and the physical maximum reported by the boiler.


###### Auxiliary Temperature

```json
    "AuxiliaryTemperature": 11.6,
```

In case your built-in outside temperature sensor is not working as you wish it would, you can send your own temperature into the system to be used for several adaptive calculations.

###### Room Reference Temperature and Target

```json
    "AmbientTemperature": 0,
    "TargetAmbientTemperature": 21,
```

These values are required by the [Fastheatup Feature](../../../../README.md#fast-heatup) 

###### Boost & Duration

```json
    "OnDemandBoost": false,
    "OnDemandBoostDuration": 600,
```

Enable or disable the [Boost Feature](../../../../README.md#boost) and set its duration in seconds.

###### Fast Heatup
```json
    "FastHeatup": false,
```

Enable or disable the [Fastheatup Feature](../../../../README.md#fast-heatup) 

###### Adaption
```json
    "Adaption": 0,
```

The manual temperature correction depends on the selected mode. In dynamic valve mode it raises the outside baseline proportionally toward the cold basepoint and fades to zero at the warm cutoff. Static valve mode retains the fixed correction in its interpolation base.

###### Valve Based Control Settings

```json
    "ValveScaling": true,
    "ValveScalingMaxOpening": 100,
    "ValveScalingOpening": 75,
```

Enable or disable valve scaling, set the maximum reported opening of your valves and the current "winning" valve's value. 

Read [Valve Scaling](../../../../README.md#valve-based-control) to understand what's going on here and how it's used.

###### Dynamic Adaption
```json
    "DynamicAdaption": true,
```

Enables or disables the [Dynamic Adaption Feature](../../../../README.md#dynamic-adaption)

###### Override Setpoint
```json
    "OverrideSetpoint": false
```

Use the requested [Feed Setpoint](#feed-setpoint) instead of the automatic curve. Physical boiler limits and disabled/zero-valve off precedence still apply.

This is considered the completely manual method where you do your own calculations and just steer the temperature to your likings.

###### Forecast Correction

The combined heating-parameter topic also accepts a transient forecast pair:

```json
{
    "ForecastCorrection": 1.5,
    "ForecastValidForSeconds": 3600
}
```

Correction must be a finite number between -2 and 2 °C. Lifetime must be an integer
from 1 to 14400 seconds; it defaults to 3600 when omitted. Lifetime without a
correction is invalid. The main loop applies the newest queued sample, removes it
at expiry, and does not persist it. Send zero to clear it. Publish these commands
without MQTT retain so an old correction is not refreshed by broker replay.
See [Forecast API](../../../API.md#get-apiforecast-and-post-apiforecast) for HTTP
ingestion, telemetry and the external weather-producer contract.


## Persistence

Stable values accepted through the combined MQTT parameter topics or Home Assistant command topics are saved to `RuntimeControls` and restored after reboot:

- `Enabled`, `FeedSetpoint`, `FeedMinimum`, `TargetAmbientTemperature`, `Adaption`, `ValveScaling`, `ValveScalingMaxOpening`, `DynamicAdaption`, `OverrideSetpoint`, and `OnDemandBoostDuration`.
- `FeedBaseSetpoint` and `FeedCutOff`, stored as the normal-operation values in `General`.
- Hot-water `Setpoint`.

Writes are coalesced for three seconds and unchanged values are not written again. Live inputs (`AuxiliaryTemperature`, `AmbientTemperature`, and `ValveScalingOpening`) and momentary state (`OnDemandBoost`/`Boost` and `FastHeatup`) remain transient. The fallback web page and `POST /api/control` are also transient by design.

See [Runtime Controls](../../../Configuration.md#runtime-controls) for the stored schema and the [API reference](../../../API.md#mqtt-api) for Home Assistant topic behavior.

## Water Parameters

- [Water Parameters](ExampleWaterParameters.json) lets you configure the temperature and behavior of your hot water circuit

###### Setpoint

```json
{
    "Setpoint": 40
}
```

This is the desired water temperature. The same rules apply like for the heating. You can't go beyond the maximum capabilities of your boiler. It will stop at the maximum allowed temperature.

## Request Parameters & Status

- [Request Status](ExampleRequestStatus.json) lets you request values on demand rather than waiting for the ESP to send it.

```json
{
    "HeatingTemperatures": true,
    "WaterTemperatures": true,
    "AuxiliaryTemperatures": true,
    "Status": true
}
```

This will trigger a value update on the individual topics. You can disable all features, for example, and time the updates on your own this way since sending this message will yield values no matter what.
