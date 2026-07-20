# Web and MQTT API Reference

This reference describes the interfaces exposed by the current development firmware. API field names are case-sensitive and the HTTP API is not versioned yet.

## Security

The controller is designed for a trusted, isolated local network. Its web server has no built-in authentication or TLS. Configuration responses can contain Wi-Fi and MQTT passwords, and several routes can change boiler commands, replace files or firmware, or reboot the controller.

Do not expose the controller directly to the internet. If remote access is required, place it behind an authenticated VPN or an independently secured gateway. Do not publish configuration responses, browser captures, or logs containing credentials.

## Conventions

- JSON requests use `Content-Type: application/json`.
- Configuration and control property names are case-sensitive.
- `200 OK` indicates a successful read or save, `202 Accepted` indicates that a configuration reload and reboot were scheduled, `400 Bad Request` indicates invalid input, and `404 Not Found` indicates a missing path. Filesystem or update failures can return `409` or `500`.
- `POST /api/control` changes the running command state but does not persist it. Stable MQTT and Home Assistant commands are persisted as described in [Configuration](Configuration.md#runtime-controls).
- Temperatures are represented internally in degrees Celsius. Home Assistant converts displayed values according to `HomeAssistant.TempUnit`, normally `°C` or `°F`.

## Runtime API

### `GET /api/runtime`

Returns the complete dashboard snapshot. This is the preferred endpoint for dashboards and integrations that need the current controller state.

```json
{
  "System": {
    "Wifi": true,
    "Mqtt": true,
    "OverrideControl": true,
    "FailSafe": false,
    "CanErrors": 0,
    "Uptime": 123456,
    "CanReadOnly": false,
    "CanProfiles": {
      "Heating": true,
      "MixedCircuit": false,
      "DomesticHotWater": true
    }
  },
  "General": {
    "FlameLit": false,
    "OutsideTemperature": 12.5,
    "HasOutsideTemperature": true,
    "Error": 0
  },
  "Heating": {
    "FeedMaximum": 75,
    "FeedCurrent": 32.5,
    "FeedSetpoint": 35,
    "CalculatedFeedSetpoint": 35,
    "Pump": true,
    "Active": true,
    "Power": 43.3,
    "Season": true,
    "Economy": false,
    "BufferMaximum": 0,
    "BufferCurrent": 0
  },
  "HotWater": {
    "Current": 48,
    "Setpoint": 50,
    "Maximum": 60,
    "Now": false,
    "Buffer": true,
    "ContinuousFlowSetpoint": 0
  },
  "MixedCircuit": {
    "Pump": false,
    "Economy": false,
    "FeedSetpoint": 0,
    "FeedCurrent": 0
  },
  "Auxiliary": [
    {"Label": "Return", "Temperature": 28.5, "Reachable": true}
  ],
  "Command": {
    "Enabled": true,
    "FeedSetpoint": 35,
    "FeedBaseSetpoint": -10,
    "FeedCutOff": 21,
    "FeedMinimum": 10,
    "AuxiliaryTemperature": 12.5,
    "AmbientTemperature": 20.5,
    "TargetAmbientTemperature": 21,
    "Adaption": 0,
    "ValveScaling": false,
    "ValveScalingMaxOpening": 100,
    "ValveScalingOpening": 0,
    "DynamicAdaption": false,
    "OverrideSetpoint": false,
    "OnDemandBoostDuration": 600,
    "Boost": false,
    "BoostTimeLeft": 0,
    "FastHeatup": false,
    "HotWaterSetpoint": 50
  }
}
```

Consumers should check the enabled installation profiles in `System.CanProfiles` before displaying mixed-circuit or domestic-hot-water sections and should not interpret an unchanged value as proof that a fresh CAN frame was received.

### `POST /api/control`

Applies a partial runtime command document and returns the same complete document as `GET /api/runtime`. Omitted fields are unchanged.

```shell
curl -X POST http://cerasmarter.local/api/control \
  -H "Content-Type: application/json" \
  -d '{"Enabled":true,"FeedBaseSetpoint":-10,"FeedCutOff":21}'
```

Accepted properties and ranges:

| Property | Type | Range or meaning |
| --- | --- | --- |
| `Enabled` | boolean | Enable heating demand |
| `FeedSetpoint` | number | 0–100 °C; effective output is clamped to the boiler-safe minimum |
| `FeedBaseSetpoint`, `FeedCutOff` | number | -50–50 °C |
| `FeedMinimum` | number | 0–100 °C |
| `AuxiliaryTemperature`, `AmbientTemperature` | number | -50–100 °C |
| `TargetAmbientTemperature` | number | 5–35 °C |
| `Adaption` | number | -30–30 °C |
| `ValveScaling`, `DynamicAdaption`, `OverrideSetpoint` | boolean | Control mode flags |
| `ValveScalingMaxOpening` | number | 1–100% |
| `ValveScalingOpening` | number | 0–100% live valve demand |
| `OnDemandBoostDuration` | number | 0–86400 seconds |
| `Boost`, `FastHeatup` | boolean | Momentary operating modes |
| `HotWaterSetpoint` | number | 0–100 °C |

A sustained effective 10 °C feed request activates the manufacturer-compatible heating-off guard after the configured grace period. Web API commands are intentionally transient; use MQTT or Home Assistant for stable values that should survive a reboot.

## System information

### `GET /api/info`

Returns chip model and revision, CPU frequency and core count, heap and application-flash usage, LittleFS usage, CAN status/error count, MQTT state, and fail-safe state.

### `GET /api/freestorage`

Returns LittleFS `Free`, `Used`, `Total`, `UsedPercent`, and `FreePercent` values in bytes/percent.

### `GET /api/wifi/network`

Returns the connected network's `ssid`, `rssi`, `ip`, `gateway`, `dns`, `mask`, and `channel`.

### `GET /api/wifi/networks`

Returns scanned networks as an array of `SSID`, `RSSI`, and `Encryption` objects. An empty array is valid while the asynchronous roaming scan owns the Wi-Fi scanner.

## Configuration API

Configuration endpoints read and update `/configuration.json`. Successful writes are atomic and refresh the persistent NVS mirror. Prefer the web forms unless an automated provisioning workflow needs these routes.

| Endpoint | Main fields | Save behavior |
| --- | --- | --- |
| `GET/POST /api/config/general` | Feature publication flags, outside-temperature override, timezone, CAN timeout, low-setpoint shutdown, normal heating curve, debug/sniffing, offline fail-safe schedule and limits | POST accepts a partial document |
| `GET/POST /api/config/wifi` | `wifi_ssid`, `wifi_pw`, `hostname` | POST requires all fields; GET exposes the password |
| `GET/POST /api/config/mqtt` | `mqtt-server`, `mqtt-port`, `mqtt-user`, `mqtt-password` | POST requires all fields; GET exposes the password |
| `GET/POST /api/config/mqtt-topics` | `status`, `statusrequest`, `auxvalues`, `boost`, `fastheatup`, `heatingparameters`, `heatingvalues`, `waterparameters`, `watervalues` | POST requires all fields |
| `GET/POST /api/config/homeassistant` | `enabled`, `discovery-prefix`, `device-id`, `temperature-unit`, `off-delay`; GET also returns `state-topic` | POST requires all editable fields and reconnects MQTT |
| `GET/POST /api/config/canbus` | Installation profiles, read-only mode, oscillator, controller detection, heartbeat, CAN addresses | POST requires the complete CAN form; addresses are 0–`0x7ff` |
| `GET/POST /api/config/leds` | `wifi-led`, `status-led`, `mqtt-led`, `heating-led` | POST requires all pins; valid range is 0–39 |

The complete persistent schema, validation details, and examples are maintained in [Configuration](Configuration.md) and the [credential-free template](Templates/Configurations/configuration.json).

The General endpoint uses the web-form field names `heatingvalues`, `watervalues`, `auxvalues`, `overrideot`, `tz`, `posix-tz`, `busmsgtimeout`, `setpoint-off-delay`, `normal-basepoint`, `normal-endpoint`, `debug`, `sniffing`, `failsafe-enabled`, `failsafe-timeout`, `failsafe-start`, `failsafe-stop`, `failsafe-unknown-time-heat`, `failsafe-basepoint`, `failsafe-endpoint`, `failsafe-minimum-feed`, and `failsafe-maximum-feed`. Its POST handler accepts a partial object; the other configuration rows marked as complete require every listed form field.

### Auxiliary-sensor compatibility shape

`GET /api/config/auxsensors` currently returns a compatibility wrapper whose first array element contains the sensor array:

```json
[[{"Label":"Return","IsReturnValue":true,"Address":["28","ff","00","00","00","00","00","01"]}]]
```

`POST /api/config/auxsensors` expects the direct sensor array without that outer wrapper:

```json
[{"Label":"Return","IsReturnValue":true,"Address":["28","ff","00","00","00","00","00","01"]}]
```

The web UI accepts both the nested compatibility response and a flat response.

### `POST /api/config/reload`

Validates `/configuration.json`, returns `202 Accepted`, and schedules a reboot to activate the file. The currently running values remain active until the reboot. Invalid files are rejected without replacing the running configuration.

## Files and updates

| Endpoint | Description |
| --- | --- |
| `GET /api/listfiles?path=/` | Lists files beneath a LittleFS path as `{path: [{Name, Size, Directory}]}` |
| `GET /filemanager/file?name=/path&action=download` | Downloads a file with its original basename and extension |
| `GET /filemanager/file?name=/path&action=delete` | Deletes a file; this route changes state despite using GET |
| `POST /filemanager/upload` | Multipart file upload; a configuration upload is staged and validated before commit |
| `POST /upload-firmware` | Multipart firmware or LittleFS upload; filenames containing `littlefs` select filesystem update handling |
| `GET /reboot` | Schedules a reboot; this route changes state despite using GET |

Uploading `/configuration.json` does not immediately replace the in-memory configuration. Call `POST /api/config/reload` or use **Reload Configuration** in the file manager. LittleFS uploads preserve valid device configuration through the NVS mirror; see [Saving, uploading, and reloading](Configuration.md#saving-uploading-and-reloading) for provisioning and factory-reset edge cases.

## CAN event stream

### `GET /events`

Opens a Server-Sent Events stream. CAN frames use the event name `can` and this payload:

```text
event: can
data: {"id":592,"len":8,"rcv":true,"ts":123456,"data":[1,2,3,4,5,6,7,8]}
```

- `id`: decimal 11-bit CAN identifier
- `len`: data length
- `rcv`: `true` for received frames and `false` for transmitted frames
- `ts`: controller uptime timestamp in milliseconds
- `data`: payload bytes

The stream powers the [CAN Analyzer](CANAnalyzer.md). Its investigation workflow, filters, cadence statistics, and export format are documented there.

## MQTT API

When Home Assistant discovery is disabled, Cerasmarter publishes the configured legacy value topics and subscribes to the combined heating/water parameter, status request, boost, and fast-heat-up topics. See the maintained [command examples](Examples/MQTT_Message_Exchange/Receive/README.md) and [state payload examples](Examples/MQTT_Message_Exchange/Send/README.md).

When Home Assistant discovery is enabled, retained state is published instead below `cerasmarter/<DeviceId>/Heating/state`, `/Water/state`, `/Auxiliary/state`, and `/General/state`. Home Assistant command topics below `cerasmarter/<DeviceId>/...` and the legacy combined command topics share the same validation and persistence rules, so existing command publishers remain usable even though state output moves to the device-specific topics.

Availability is retained at `cerasmarter/<DeviceId>/availability`; the MQTT Last Will publishes `offline`, and a successful connection publishes `online`. The controller republishes discovery after receiving Home Assistant's birth message on `<AutoDiscoveryPrefix>/status`.
