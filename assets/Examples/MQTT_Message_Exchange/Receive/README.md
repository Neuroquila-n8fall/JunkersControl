# What's this?
These are the example messages which can be processed by the application. These messages will, for example, trigger a message which is then received by your MQTT broker.

## Heating Parameters

- [Heating Parameters](ExampleHeatingParameters.json) lets you configure the temperatures, behavior of your heating circuit and operation mode

```json
{
    "Enabled": false,
    "FeedSetpoint": 0,
    "FeedBaseSetpoint": -10,
    "FeedCutOff": 22,
    "FeedMinimum": 10,
    "AuxilaryTemperature": 11.6,
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
    "FeedSetpoint": 0,
```

The feed temperature setpoint when ...
```json
    "OverrideSetpoint": true
```

override is enabled.

###### Base Setpoint (Weather Guided)

```json
    "FeedBaseSetpoint": -10,
```

The heating will yield 100% of it's heating power (feed temperature) when outside temperature is at or below this setting.

###### Cutoff Temperature (Weather Guided)

```json
    "FeedCutOff": 22,
```

The heating will stop operation when the outside temperature has reached this value.

###### Anti-Freeze

```json
    "FeedMinimum": 10,
```

This is a more or less static value determined by your heating system. If the feed temperature goes below this point, the heating will resume operation to prevent pipes from freezing. You can now use this how you like but you can't go below 10°C


###### Auxilary Temperature

```json
    "AuxilaryTemperature": 11.6,
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

The feed calculations are off? Just set a value here and it will adjust the final setpoint accordingly.

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

Ignore and Override any internal calculations and set the feed to the temperature set by [Feed Setpoint](#feed-setpoint)

This is considered the completely manual method where you do your own calculations and just steer the temperature to your likings.

Be aware, though, that you can only "suggest" temperatures to the heating. It will throttle itself to prevent any harm and malfunction on its own. It actively prevents setting unrealistic values that might break the boundaries of its internal parameters. So for example if you set the feed to 100°C but your system is only able to deliver 90°C, it will set 90°C. If the dial is set to allow 75°C, it will only go as far as 75°C.

## Water Parameters

- [Water Parameters](ExampleWaterParameters.json) lets you configure the temperature and behavior of your hot water circuit

## Request Parameters & Status

- [Request Status](ExampleRequestStatus.json) lets you request values on demand rather than waiting for the ESP to send it.