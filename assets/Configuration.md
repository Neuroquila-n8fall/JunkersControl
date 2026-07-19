# Configuration File Explained

## Template

Start with the [Configuration Template](Templates/Configurations/configuration.json), customize it, and upload it as `/configuration.json` through the web file manager. Then click **Reload Configuration** to validate and activate the uploaded file.

Do not store a real configuration in the repository's `data` folder. Device configurations commonly contain WiFi and MQTT credentials. The filesystem pipeline copies the credential-free template into the image as `/configuration.json` instead.

## Saving, uploading, and reloading

Configuration forms in the web UI save directly to `/configuration.json`. Saves use a temporary file, verification, and a backup so an incomplete write does not replace the last valid configuration.

Uploading a complete `/configuration.json` does not merge it with the current in-memory values. Use **Reload Configuration** in the file manager after the upload. If the file is invalid, the controller keeps the current configuration and reports the error.

For compatibility with older configuration files, settings added by newer firmware are copied from the running device only when the corresponding key is absent from the uploaded file. Explicit uploaded values always win. For example, an omitted `FailSafe.EndpointTemperature` retains the current endpoint, while `HomeAssistant.Enabled: false` deliberately disables discovery after the configuration is reloaded. Until reload or reboot, configuration pages continue showing the currently loaded in-memory values rather than the staged upload.

Every valid configuration is mirrored to NVS after it is loaded or saved. If LittleFS is replaced through the WebUI, PlatformIO, esptool, or a programmer, startup recognizes the credential-free provisioning template and restores the persistent copy automatically. A deliberately uploaded valid configuration takes precedence and refreshes the mirror after loading. Keep a downloaded copy for recovery from a full-flash or NVS erase.

The filesystem pipeline records intent with the root-level `ProvisioningTemplate` field. Release images use `true`; an external configuration selected through `CERASMARTER_CONFIG_FILE` is automatically stamped `false` and therefore takes precedence over an older NVS mirror. Set the marker explicitly when constructing images outside the supplied pipeline. Never publish a custom image containing device credentials.

For an intentional factory reset, hold the BOOT button continuously for 10 seconds during startup. This clears both the persistent NVS mirror and the LittleFS configuration copies, retains the web frontend, and enters Setup Mode. A normal short BOOT press still enters Setup Mode without deleting anything.

### Wifi

This block is for configuring the Wifi network your controller should connect to.

```json
    "Wifi": {
        "SSID": "ssid",
        "Password": "pass",
        "Hostname": "CERASMARTER"
    },
```

The hostname is applied before the WiFi connection is started. If local hostname resolution is unavailable on your network, connect using the address assigned by DHCP.

### MQTT

This block configures the connection to your MQTT broker

```json
    "MQTT": {
        "Server": "1.2.3.4",
        "Port": 1883,
        "User": "mqtt",
        "Password": "mqtt",
        "Topics": {
            "HeatingValues": "cerasmarter/heating/values",
            "HeatingParameters": "cerasmarter/heating/parameters",
            "WaterValues": "cerasmarter/water/values",
            "WaterParameters": "cerasmarter/water/parameters",
            "AuxiliaryValues": "cerasmarter/auxiliary/values",
            "Status": "cerasmarter/status",
            "StatusRequest": "cerasmarter/status/get"
        }
    },
```

#### Connection

```json
        "Server": "1.2.3.4",
        "Port": 1883,
        "User": "mqtt",
        "Password": "mqtt",
```
These should be self-explanatory.

#### Topics

```json
        "Topics": {
            "HeatingValues": "cerasmarter/heating/values",
            "HeatingParameters": "cerasmarter/heating/parameters",
            "WaterValues": "cerasmarter/water/values",
            "WaterParameters": "cerasmarter/water/parameters",
            "AuxiliaryValues": "cerasmarter/auxiliary/values",
            "Status": "cerasmarter/status",
            "StatusRequest": "cerasmarter/status/get"
        }
```

##### Getters (Publishers)

These topics are used by the program to send data.

```json
            "HeatingValues": "cerasmarter/heating/values",
            "WaterValues": "cerasmarter/water/values",
            "AuxiliaryValues": "cerasmarter/auxiliary/values",
            "Status": "cerasmarter/status",
```

##### Setters (Subscriptions)

The ESP will subscribe to these topics in order to receive parameters.

```json
            "HeatingParameters": "cerasmarter/heating/parameters",
            "WaterParameters": "cerasmarter/water/parameters",
            "StatusRequest": "cerasmarter/status/get"
```

###### Status Request

```json
            "StatusRequest": "cerasmarter/status/get"
```

This topic is used for requesting data on-demand. See [Status Request Explanation](Examples/MQTT_Message_Exchange/Receive/README.md)

### Features

```json
    "Features": {
        "HeatingParameters": true,
        "WaterParameters": false,
        "AuxiliaryValues": false
    },
```

Enables automatic transmission for each parameter-set. You can always trigger an update using a [Status Request](#status-request)

### Time

This block is for time related configuration

```json
    "Time": {
        "Timezone": "Europe/Berlin",
        "PosixTimezone": "CET-1CEST,M3.5.0,M10.5.0/3"
    }
```

`Timezone` identifies the configured region. `PosixTimezone` supplies local UTC-offset and daylight-saving rules without a network lookup, allowing the fail-safe schedule to keep local time after internet loss.

### Offline fail-safe

```json
    "FailSafe": {
        "Enabled": true,
        "CommandTimeoutSeconds": 300,
        "StartHour": 5,
        "StartMinute": 30,
        "StopHour": 23,
        "StopMinute": 30,
        "HeatWhenTimeUnknown": true,
        "BasepointTemperature": -10.0,
        "EndpointTemperature": 31.0,
        "MinimumFeedTemperature": 10.0,
        "MaximumFeedTemperature": 55.0
    }
```

For configuration-file compatibility, `CommandTimeoutSeconds` retains its existing name. It defines the grace period for a continuous MQTT disconnection; the frequency of heating commands does not affect fail-safe mode. When the grace period expires, the current runtime controls are saved and the local profile is applied. MQTT reconnection restores those controls and leaves fail-safe mode, without requiring a fresh command or a Node-RED-style heartbeat.

The schedule uses boiler-reported time first, then locally synchronized time. With no valid clock it applies `HeatWhenTimeUnknown`: `true` holds the minimum feed temperature, while `false` disables heating.

### General Settings

Control some global settings

```json
    "General": {
        "BusMessageTimeout": 30,
        "SetpointOffDelaySeconds": 300,
        "Debug": false,
        "Sniffing": false
    },
```

###### Bus Message Timeout

```json
        "BusMessageTimeout": 30,
```

Specifies the time in seconds(!) when we should take over control over the system after the last message from a TAxxx Controller has been received. 

The program will stop issuing control messages immediately after another controller has been seen on the bus. If the foreign controller stops sending messages for `30` seconds, we will take over control

###### Low-setpoint Off Delay

```json
        "SetpointOffDelaySeconds": 300,
```

The manufacturer-defined 10 °C (50 °F) effective feed setpoint means heating off. Calculated values below 10 °C are clamped to exactly 10 °C before CAN conversion and transmission. If that request remains continuously active for this many seconds, Cerasmarter disables the heating operation flag. The controller-side latch applies to every command transport and prevents repeated `Enabled: true` messages from defeating the shutdown. An effective setpoint above 10 °C clears the latch; a client that explicitly manages `Enabled` should send `Enabled: true` with the higher demand. Values from 10 through 86400 seconds are accepted.

###### Debug

```json
        "Debug": false,
```

This will output debug messages on the console. Usually only warnings and errors will be sent to the console but if you face a problem you might want to see what's going on under the hood.

Example Output:

```log
[18-Sep-22 10:24:45.905] - CAN: [0x201] Data:   0x25 (37)
[18-Sep-22 10:24:46.540] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 10:24:47.540] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 10:24:48.040] - CAN: [0x20A] Data:   0x00 (0)
DEBUG STEP CHAIN #2: Heating is ON, Fail-safe is NO, Feed Setpoint is 10.00, INT representation (half steps) is 20
[18-Sep-22 10:24:51.040] - CAN: [0x201] Data:   0x25 (37)
[18-Sep-22 10:24:52.540] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 10:24:53.540] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 10:24:54.040] - CAN: [0x20A] Data:   0x00 (0)
[18-Sep-22 10:24:54.251] - CAN: [0x0F9] Data:
[18-Sep-22 10:24:55.271] - CAN: [0x250] Data:   0x01 (1)
[18-Sep-22 10:24:57.040] - CAN: [0x201] Data:   0x27 (39)
[18-Sep-22 10:24:58.540] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 10:24:59.540] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 10:25:00.040] - CAN: [0x20A] Data:   0x00 (0)
DEBUG STEP CHAIN #0: Heating Economy: 1
[18-Sep-22 10:25:03.040] - CAN: [0x201] Data:   0x25 (37)
[18-Sep-22 10:25:04.540] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 10:25:05.540] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 10:25:06.040] - CAN: [0x20A] Data:   0x00 (0)
DEBUG: Date and Time DOW:7 H:10 M:25
DEBUG STEP CHAIN #2: Heating is ON, Fail-safe is NO, Feed Setpoint is 10.00, INT representation (half steps) is 20
[18-Sep-22 10:25:09.040] - CAN: [0x201] Data:   0x25 (37)
[18-Sep-22 10:25:10.240] - CAN: [0x204] Data:   0x14 (20)
[18-Sep-22 10:25:10.540] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 10:25:11.240] - CAN: [0x206] Data:   0x00 (0)
[18-Sep-22 10:25:11.540] - CAN: [0x209] Data:   0x00 (0)

```

###### Sniffing

Outputs the received messages from the bus to the console

```json
        "Sniffing": true
```

Example Output:

```log
[18-Sep-22 08:27:58.548] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 08:27:59.549] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 08:28:00.049] - CAN: [0x20A] Data:   0x00 (0)
[18-Sep-22 08:28:03.048] - CAN: [0x201] Data:   0x25 (37)
[18-Sep-22 08:28:04.548] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 08:28:05.549] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 08:28:06.048] - CAN: [0x20A] Data:   0x00 (0)
[18-Sep-22 08:28:09.048] - CAN: [0x201] Data:   0x25 (37)
[18-Sep-22 08:28:10.248] - CAN: [0x204] Data:   0x14 (20)
[18-Sep-22 08:28:10.548] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 08:28:11.248] - CAN: [0x206] Data:   0x00 (0)
[18-Sep-22 08:28:11.548] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 08:28:11.749] - CAN: [0x207] Data:   0x04 (4)        0x5D (93)
[18-Sep-22 08:28:12.048] - CAN: [0x20A] Data:   0x00 (0)
[18-Sep-22 08:28:12.248] - CAN: [0x200] Data:   0x46 (70)
[18-Sep-22 08:28:12.749] - CAN: [0x20C] Data:   0x00 (0)
[18-Sep-22 08:28:13.039] - CAN: [0x0DF] Data:   0x00 (0)
[18-Sep-22 08:28:13.248] - CAN: [0x208] Data:   0x00 (0)
[18-Sep-22 08:28:15.048] - CAN: [0x201] Data:   0x25 (37)
[18-Sep-22 08:28:16.548] - CAN: [0x20D] Data:   0x18 (24)
[18-Sep-22 08:28:17.548] - CAN: [0x209] Data:   0x00 (0)
[18-Sep-22 08:28:18.048] - CAN: [0x20A] Data:   0x00 (0)

```

Following the timestamp when the message has been received, you will find the ID of the message, i.e.: `CAN: [0x20D]` following the data bytes in hexadecimal representation and the decimal value in paranthesis `0x18 (24)`. Each Byte is separated by tab `\t`


### Home Assistant

Cerasmarter supports native MQTT device discovery. No manual Home Assistant YAML or filesystem discovery-template files are needed.

```json
    "HomeAssistant": {
        "AutoDiscoveryPrefix": "homeassistant",
        "OffDelay": 30,
        "Enabled": true,
        "DeviceId": "cerasmarter_1",
        "TempUnit": "°C"
    }
```

- `Enabled` activates Home Assistant device discovery and Home Assistant-specific state and command topics.
- `AutoDiscoveryPrefix` must match the discovery prefix configured in Home Assistant's MQTT integration. Its default is `homeassistant`.
- `DeviceId` must be unique for every controller on the broker. It identifies the Home Assistant device and forms part of its MQTT topics. Spaces and `/` characters are normalized to `_`.
- `TempUnit` is used by all discovered temperature entities. Normally this is `°C` or `°F`.
- `OffDelay` is the number of seconds after which an active binary sensor returns to off without a newer active state. Set it to `0` to disable this behavior.

The controller publishes one retained device-discovery payload to `<AutoDiscoveryPrefix>/device/<DeviceId>/config`. Runtime data uses `cerasmarter/<DeviceId>/...`; changing the discovery prefix does not change state or command topics.

Treat `DeviceId` as stable after Home Assistant has discovered the controller. When the ID or discovery prefix is changed through the web interface, the controller removes its previous retained discovery record before reconnecting with the new identity. If the broker is unavailable during that change, remove the old device or retained discovery topic manually.

The discovered device contains:

- General error and gas-burner state.
- Heating temperatures, pump/season/operation/boost states, and fast-heatup state.
- Hot-water temperatures and operating states.
- A temperature entity and diagnostic connectivity entity for every configured auxiliary sensor.
- Diagnostic entities for heap memory, filesystem and flash storage, chip model and revision, and CPU core count and frequency.
- Number controls for requested feed temperature, boost duration, and room-reference temperature.
- Switch controls for heating enablement, boost, and fast heatup.

Discovery and state payloads are retained. The controller publishes retained online/offline availability using MQTT Last Will and republishes discovery when Home Assistant sends its MQTT birth message.

### CAN Configuration

```json
    "CAN": {
        "Quartz": 16,
        "Addresses": {
            "Controller": {
                "FlameStatus": "0x209",
                "Error": "0x206",
                "DateTime": "0x256"
            },
            "Heating": {
                "FeedCurrent": "0x201",
                "FeedMax": "0x200",
                "FeedSetpoint": "0x252",
                "OutsideTemperature": "0x207",
                "Pump": "0x20A",
                "Season": "0x20C",
                "Operation": "0x250",
                "Power": "0x251"
            },
            "HotWater": {
                "SetpointTemperature": "0x203",
                "MaxTemperature": "0x204",
                "CurrentTemperature": "0x205",
                "Now": "0x254",
                "BufferOperation": "0x20B",
                "ContinousFlow": {
                    "SetpointTemperature": "0x255"
                }
            },
            "MixedCircuit": {
                "Pump": "0x404",
                "FeedSetpoint": "0x405",
                "FeedCurrent": "0x440",
                "Economy": "0x407"
            }
        }
    },
```

###### Quartz Frequency

```json
        "Quartz": 16,
```
Your CAN-Module might have a 8MHz or 16MHz oscillator (quartz) installed. Adjust the frequency here accordingly.

###### Addresses

```json
        "Addresses": {
            "Controller": {
                "FlameStatus": "0x209",
                "Error": "0x206",
                "DateTime": "0x256"
            },
            "Heating": {
                "FeedCurrent": "0x201",
                "FeedMax": "0x200",
                "FeedSetpoint": "0x252",
                "OutsideTemperature": "0x207",
                "Pump": "0x20A",
                "Season": "0x20C",
                "Operation": "0x250",
                "Power": "0x251"
            },
            "HotWater": {
                "SetpointTemperature": "0x203",
                "MaxTemperature": "0x204",
                "CurrentTemperature": "0x205",
                "Now": "0x254",
                "BufferOperation": "0x20B",
                "ContinousFlow": {
                    "SetpointTemperature": "0x255"
                }
            },
            "MixedCircuit": {
                "Pump": "0x404",
                "FeedSetpoint": "0x405",
                "FeedCurrent": "0x440",
                "Economy": "0x407"
            }
        }
```

This is where things get a little bit complicated. The addresses defined here might be different to what your system sends. If something is off and you found the correct ID using [Sniffing Setting](#sniffing) you can adjust it using these blocks.

### Auxiliary Sensors

You can setup your own set of external sensors to be sent on the mqtt topic [Auxiliary Temperatures](Examples/MQTT_Message_Exchange/Send/README.md#auxiliary-sensors)

```json
    "AuxiliarySensors":
    {
        "Count": 4,
        "Sensors":
        [
            {
                "Label": "Feed",
                "IsReturnValue": false,
                "Address":         
                [
                    "0x28", "0x76", "0x51", "0x91", "0x42", "0x20", "0x01", "0xE3"
                ]                
            },
            {
                "Label": "Return",
                "IsReturnValue": true,
                "Address":
                [
                    "0x28", "0x6F", "0x9C", "0xF6", "0x42", "0x20", "0x01", "0xF6"
                ]
            },
            {
                "Label": "Exhaust",
                "IsReturnValue": false,
                "Address":
                [
                    "0x28", "0x6D", "0x98", "0xF5", "0x42", "0x20", "0x01", "0x0F"
                ]
            },
            {
                "Label": "Ambient",
                "IsReturnValue": false,
                "Address":
                [
                    "0x28", "0xBF", "0x39", "0x10", "0x42", "0x20", "0x01", "0x93"
                ]
            }
        ]
    },
```

**IMPORTANT**
The `Count` has to be less or equal the amount of sensor you have defined under `Sensors` or else the program will crash!

Each sensor is defined by a label, if it's used to reference the return temperature and its address.
In the following example the sensor is called "Feed" (`"Label": "Feed"`) and shouldn't be used as a reference temperature (`"IsReturnValue": false`) in [Dynamic Adaption](../README.md#dynamic-adaption) Mode.

Its address is set as an array of the string representation of the hex values
```
                [
                    "0x28", "0x76", "0x51", "0x91", "0x42", "0x20", "0x01", "0xE3"
                ]  
```

The completed sensor block looks like this:
```json
            {
                "Label": "Feed",
                "IsReturnValue": false,
                "Address":         
                [
                    "0x28", "0x76", "0x51", "0x91", "0x42", "0x20", "0x01", "0xE3"
                ]                
            },
```

Full example with 4 sensors:
```json
    "AuxiliarySensors":
    {
        "Count": 4,
        "Sensors":
        [
            {
                "Label": "Feed",
                "IsReturnValue": false,
                "Address":         
                [
                    "0x28", "0x76", "0x51", "0x91", "0x42", "0x20", "0x01", "0xE3"
                ]                
            },
            {
                "Label": "Return",
                "IsReturnValue": true,
                "Address":
                [
                    "0x28", "0x6F", "0x9C", "0xF6", "0x42", "0x20", "0x01", "0xF6"
                ]
            },
            {
                "Label": "Exhaust",
                "IsReturnValue": false,
                "Address":
                [
                    "0x28", "0x6D", "0x98", "0xF5", "0x42", "0x20", "0x01", "0x0F"
                ]
            },
            {
                "Label": "Ambient",
                "IsReturnValue": false,
                "Address":
                [
                    "0x28", "0xBF", "0x39", "0x10", "0x42", "0x20", "0x01", "0x93"
                ]
            }
        ]
    },
```

**IMPORTANT**
If you set more than one sensor as `"IsReturnValue": true` the last one on the list will be the actual reference. Make sure you only set the one desired sensor as a reference.

### LEDs

You can setup LEDs to reflect the status of the system.

```json
    "LEDs": {
        "Wifi": 26,
        "Status": 27,
        "Mqtt": 14,
        "Heating": 25
    }
```

The numbers refer to the GPIO which is printed on the board of your ESP module.
