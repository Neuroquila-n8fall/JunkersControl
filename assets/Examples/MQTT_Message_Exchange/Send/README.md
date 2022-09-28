# Whats this?
These are the messages which are sent to your MQTT broker

## Heating Values
- [Heating Values](ExampleHeatingValues.json) contains everything related to the heating circuit

### Example Json
```json
{
    "FeedMaximum": 75.10,
    "FeedCurrent": 30.10,
    "FeedSetpoint": 10.10,
    "Outside": 15.10
}
```

#### Feed Maximum Value
```json
    "FeedMaximum": 75.10,
```
The maximum temperature the heating circuit can achieve. This is set by the pyhsical dial on your heating.

#### Current Feed Temperature
```json
    "FeedCurrent": 30.10,
```
The current feed temperature as reported by the NTC inside the heating circuit

#### Feed Setpoint
```json
    "FeedSetpoint": 10.10,
```
This is the temperature the feed should be at. 10°C usually means the HEating should stop operation (Anti-Freeze). The heating will start and heat up the circuit if the feed temperature climbs below this value.

#### Outside Temperature 
```json
    "Outside": 15.10
```
This is the temperature on the outside if your system has an outside temperature sensor (which it should have!)

## Water Values
- [Water Values](ExampleWaterValues.json) contains everything related to the water circuit

### Example Json
```json
{
    "Maximum": 40.10,
    "Current": 30.10,
    "Setpoint": 10.10,
    "CFSetpoint": 20.00,
    "Now": true,
    "Buffer": false
}
```

#### Maximum Temperature
```json
    "Maximum": 40.10,
```
This is the peak temperature the water can reach. It is set by the physical water dial on your heating.

#### Current Water Temperature
```json
    "Current": 30.10,
```
This is what your water temperature sits at right now. It might be the peak temperature of your buffer or the currently available temperature in the pipe when running in continous flow mode.

#### Setpoint
```json
    "Setpoint": 10.10,
```
This is the desired hot water temperature. It can't be higher than [the maximum temperature](#maximum-temperature)

#### Continous-Flow Setpoint
```json
    "CFSetpoint": 20.00,
```
Your system might be running as a so called continous-flow setup where there is no buffer but instead water is heated up while it flows through the boiler when you need it. This is the setpoint for this mode of operation.

#### Now/Instant Mode
```json
    "Now": true,
```
Honestly it's not quite clear what this setting is telling us. Most likely it's got something to do with instant demand for hot water but as of now clarification is required.

#### Buffer Mode
```json
    "Buffer": false
```
This setting is telling you whether the heating circuit runs in buffer mode or not, which means water is stored and monitored inside a dedicated buffer.

## Auxilary Sensors

- [Auxilary Values](ExampleAuxValues.json) contains temperatures of your externally attached sensors. Pay special attention to the [Configuration](../../../../data/configuration.json.template#L68) for setting it up

### Example JSON

```json
{
    "Feed": 30.10,
    "Return": 30.10,
    "Exhaust": 50.10,
    "Ambient": 17.10
}
```

### Explanation
These values are directly influenced by the configuration you provided which has been mentioned earlier.


## General/Overall Status

- [Status](ExampleStatus.json) contains the general status of your heating system

### Example JSON
```json
{
    "GasBurner": true,
    "Pump": true,
    "Error": 0,
    "Season": true,
    "Working": true,
    "Boost": true,
    "FastHeatup": true			
}
```

#### Gas Burner (the actual heat source)
```json
    "GasBurner": true,
```
Tells you if gas is literally burning and therefore if water or heating circuit receive an increase in temperature.

#### Pump
```json
    "Pump": true,
```
This means the pump is running or not. If the heating is going into economy mode the pump will run for a few minutes until it's going off, too. It will occasionally enable itself to prevent it from becoming stuck, if it's been disabled for 24h.

#### Error Flag
```json
    "Error": 0,
```
Decimal value of hexadecimal error code. `0` Means Operational. Other error values may vary between models.

Some common errors are:
- `168` = `A8` means the can bus is down
- `204` = `CC` means the outside temperature NTC is disconnected

#### Season
```json
    "Season": true,
```

- `true` means Winter
- `false` means Summer

This is set by the physical dial on the heating. If it's set to the lowest possible setting, this will yield `false` for Summer.

#### "On" or "Working" State
```json
    "Working": true,
```
This means the controller is in ready-state. This has nothing to do with actual heating or water ircuit operation. It is the overall state of the system.

#### Boost
```json
    "Boost": true,
```
Tells about the state of the boost function of our solution. See [Boost Function](../../../../README.md#boost)

#### Fast Heatup
```json
    "FastHeatup": true
```
Tells about the status of the "Fast Heatup" function. See [Fast Heatup](../../../../README.md#fast-heatup)