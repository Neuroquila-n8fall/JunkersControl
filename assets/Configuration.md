# Configuration File Explained

## Template

See [Configuration Template](../data/configuration.json.template)

### Wifi

This block is for configuring the Wifi network your controller should connect to.

```json
    "Wifi": {
        "SSID": "ssid",
        "Password": "pass",
        "Hostname": "CERASMARTER"
    },
```

Note: The `Hostname` setting is bugged due to some problem with the framework. The device will be called `esp-<mac>` until the problem has been solved.

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
            "AuxilaryParameters": "cerasmarter/auxilary/values",
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
            "AuxilaryParameters": "cerasmarter/auxilary/values",
            "Status": "cerasmarter/status",
            "StatusRequest": "cerasmarter/status/get"
        }
```

##### Getters (Publishers)

These topics are used by the program to send data.

```json
            "HeatingValues": "cerasmarter/heating/values",
            "WaterValues": "cerasmarter/water/values",
            "AuxilaryParameters": "cerasmarter/auxilary/values",
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
        "AuxilaryParameters": false
    },
```

Enables automatic transmission for each parameter-set. You can always trigger an update using a [Status Request](#status-request)

### Time

This block is for time related configuration

```json
    "Time": {
        "Timezone": "Europe/Berlin"
    },
```

###### Timezone

```json
        "Timezone": "Europe/Berlin"
```

This lets you configure the timezone you're in.
See [List of Timezones on Wikipedia](https://en.wikipedia.org/wiki/List_of_tz_database_time_zones)

### General Settings

Control some global settings

```json
    "General": {
        "BusMessageTimeout": 30,
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
DEBUG STEP CHAIN #2: Heating is ON, Fallback is NO, Feed Setpoint is 10.00, INT representation (half steps) is 20
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
DEBUG STEP CHAIN #2: Heating is ON, Fallback is NO, Feed Setpoint is 10.00, INT representation (half steps) is 20
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

### Auxilary Sensors

You can setup your own set of external sensors to be sent on the mqtt topic [Auxilary Temperatures](Examples/MQTT_Message_Exchange/Send/README.md#auxilary-sensors)

```json
    "AuxilarySensors":
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
    "AuxilarySensors":
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