# MQTT State Messages

Cerasmarter publishes heating, hot-water, auxiliary-sensor, and general status documents. The payload envelope and retention depend on whether Home Assistant discovery is enabled.

## Topic modes

When `HomeAssistant.Enabled` is `false`, the objects below are published directly and non-retained on the configured legacy topics:

- `MQTT.Topics.HeatingValues`
- `MQTT.Topics.WaterValues`
- `MQTT.Topics.AuxiliaryValues`
- `MQTT.Topics.Status`

When `HomeAssistant.Enabled` is `true`, each object is wrapped in its category and published retained:

| Category | Topic | Payload envelope |
| --- | --- | --- |
| Heating | `cerasmarter/<DeviceId>/Heating/state` | `{"Heating": {...}}` |
| Hot water | `cerasmarter/<DeviceId>/Water/state` | `{"Water": {...}}` |
| Auxiliary sensors | `cerasmarter/<DeviceId>/Auxiliary/state` | `{"Auxiliary": {...}}` |
| General/diagnostics | `cerasmarter/<DeviceId>/General/state` | `{"General": {...}}` |

The example files show the unwrapped legacy form. Boolean state properties are serialized as the strings `"true"` and `"false"` for compatibility with existing dashboards and Home Assistant templates.

## Heating values

[ExampleHeatingValues.json](ExampleHeatingValues.json) contains decoded heating values and a mirror of the current command state.

Important fields:

- `FeedMaximum`: maximum feed temperature reported by the boiler.
- `FeedCurrent`: current boiler feed temperature.
- `FeedSetpoint`: effective setpoint. It is `null` until a CAN setpoint has been observed when Cerasmarter is not controlling the bus.
- `RequestedFeedSetpoint`: last requested direct setpoint, which can differ from the effective calculated setpoint.
- `Outside`: boiler or configured auxiliary outside temperature.
- `Pump`, `Season`, `Working`: decoded heating operating states.
- `Enabled`, `FeedBaseSetpoint`, `FeedCutOff`, `FeedMinimum`, `Adaption`, `DynamicAdaption`, `OverrideSetpoint`: current stable command state.
- `AuxiliaryTemperature`, `AmbientTemperature`, `TargetAmbientTemperature`: external and room-reference inputs.
- `ValveScaling`, `ValveScalingMaxOpening`, `ValveScalingOpening`: valve-based control state.
- `Boost`, `BoostTimeLeft`, `OnDemandBoostDuration`, `FastHeatup`: boost and fast-heat-up state.

An effective 10 °C setpoint represents the manufacturer's heating-off request. Internally calculated values below it are clamped to exactly 10 °C.

## Hot-water values

[ExampleWaterValues.json](ExampleWaterValues.json) contains:

- `Maximum`: maximum temperature allowed by the boiler dial.
- `Current`: current hot-water temperature.
- `Setpoint`: current setpoint; `null` until observed when Cerasmarter is not controlling the bus.
- `CFSetpoint`: continuous-flow setpoint; `null` until observed.
- `Now`: instant-demand state.
- `Buffer`: storage-buffer operating state.

## Auxiliary sensors

[ExampleAuxValues.json](ExampleAuxValues.json) contains one property per configured sensor label. Each sensor has a numeric `Temperature` and string-valued `Reachable` state:

```json
{
  "Return": {
    "Temperature": 30.1,
    "Reachable": "true"
  }
}
```

Labels and DS18B20 addresses are configured as described in [Auxiliary Sensors](../../../Configuration.md#auxiliary-sensors).

## General status and diagnostics

[ExampleStatus.json](ExampleStatus.json) contains the burner and controller diagnostics:

- `GasBurner`: decoded flame state.
- `Error`: decimal representation of the boiler error byte; `0` normally means no reported error.
- `FreeHeap`, `HeapSize`: current and total heap bytes.
- `FilesystemUsed`, `FilesystemSize`: LittleFS usage in bytes.
- `FlashSize`: detected flash capacity in bytes.
- `ChipModel`, `ChipRevision`, `CpuCores`, `CpuFrequency`: ESP32 runtime information.

Pump, season, heating-operation, boost, and fast-heat-up states belong to the heating payload, not the general payload.

## Availability and cadence

With Home Assistant enabled, availability is retained at `cerasmarter/<DeviceId>/availability`. MQTT Last Will publishes `offline`; a successful connection publishes `online`. Availability is tied to the MQTT connection and is independent of how frequently a CAN value changes.

Some CAN-backed values are published on their normal polling cadence while others only change when the boiler reports a new frame. Consumers should use availability for connection health and timestamps/history for measurement freshness. A combined status request can ask Cerasmarter to republish its current state; see [Request Parameters & Status](../Receive/README.md#request-parameters--status).

See the [API reference](../../../API.md#mqtt-api) for command topics, discovery behavior, and security notes.
