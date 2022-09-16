# JunkersControl

## Table of contents
- [JunkersControl](#junkerscontrol)
  - [Table of contents](#table-of-contents)
  - [Purpose and Aim](#purpose-and-aim)
    - [Cerasmart-er](#cerasmart-er)
  - [Contribution](#contribution)
  - [Intended Audience](#intended-audience)
  - [A word of warning](#a-word-of-warning)
    - [But why? We are talking about a data line!](#but-why-we-are-talking-about-a-data-line)
  - [Prerequisites](#prerequisites)
  - [Features](#features)
    - [MQTT](#mqtt)
      - [Example Parameter JSON for setting Heating Parameters:](#example-parameter-json-for-setting-heating-parameters)
    - [Heating Parameters](#heating-parameters)
    - [Night/Economy Mode](#nighteconomy-mode)
      - [Option #1 (MQTT):](#option-1-mqtt)
      - [Option #2 (Amend Code):](#option-2-amend-code)
    - [Switch Off/On](#switch-offon)
    - [Boost](#boost)
    - [Fast Heatup](#fast-heatup)
    - [Fallback and Failsafe](#fallback-and-failsafe)
    - [Automatic Controller Detection](#automatic-controller-detection)
    - [External Temperature Sensors](#external-temperature-sensors)
      - [Configuration](#configuration)
    - [Dynamic Adaption](#dynamic-adaption)
    - [Calculate yourself](#calculate-yourself)
    - [Valve-based control](#valve-based-control)
    - [OTA Updates and Console](#ota-updates-and-console)
  - [Hints](#hints)
  - [Configuration / Setup](#configuration--setup)
  - [Dedicated PCB](#dedicated-pcb)
  - [Todo](#todo)
  - [Special Thanks](#special-thanks)

![Alt_Text](/assets/example_ha_dashboard.jpg)

#NOTE: Documentation is being updated for recent changes.

## Purpose and Aim
This project is designed around the idea of having a SCADA-like setup where your command & control server (MQTT-Broker) sends commands and receives the status of the heating.
Since the rise of modern and affordable "Smart Radiator Thermostats" we are able to precisely control the room temperature whereas the usual central heating system is only able to react on outside temperatures and doesn't know what the actual demand is like. The heating is only capable of reacting to certain drops in feed and return temperatures and heat up according to setpoints defined by the consumer. This principle is proven and hasn't changed until today.

### Cerasmart-er
The Junkers/Bosch "Heatronic" controller of the "Cerasmart" type of boilers (yes they used that term back in pre-2000!) is by itself smart enough to keep your home warm without wasting too much energy, if properly equipped and configured, of course. You won't receive any benefits from this project if you are messing around with the parameters without knowing the concepts of a central heating system. Also if your heating isn't "tuned" to your home it will waste money nonetheless.
**This also means you can't tell the boiler to perform unreasonable actions because the controller inside the boiler unit is in charge of controlling the actual temperatures within a safe, predefined range that has been set either by the manufacturer or the technician that maintains your heating.**
We are, hoewever, able to suggest certain temperatures or switch the boiler on and off

With this project we can at least account for seasonal changes in temperature, humidity and the temperature as we feel it so we can adjust the power demand to what we actually need.


## Contribution
There are a lot of different possible setups around and I am happy to accept PRs no matter what they are about. 

Just a few examples:


- You have found an Id and its meaning
- Something is explained wrong
- Bugs, of course

## Intended Audience
Since every setup is different you have to customize the data processing for yourself. I have sourced the message ids from https://www.mikrocontroller.net/topic/81265 but only process those that are relevant to me.
This means you should bring a little bit of coding experience with you.


## A word of warning
You should never, ever manipulate a device that's running on highly volatile substances if you don't know what you are doing. Nobody, in fact, is qualified to work on such a gas heating system but the trained technician that has the right tools and knowledge to modify your heating in a safe manner (read: without blowing up you, himself and the surrounding home). 
If your are missing a cable to plug in your self-made controller, don't install it yourself. Ask your qualified, local heating technician to do it.

### But why? We are talking about a data line!
 Simply because installing the bus module or attaching the data cable requires you to open the boiler, remove the cover and also the bezel that covers up the mains voltage supply. In some countries this will also void any insurance coverage if you do it yourself.

## Prerequisites
Now that we have sorted out the serious bits, lets check if we got everything together to pull this off...

1) A compatible Bosch-Junkers central gas heating system with Bosch Heatronic controller and BM1 or BM2 Bus Module equipped.
2) Access to the data line that exits the bus module. Most of the time you will find a "room thermostat" like TA250 or TA270 which in fact is the control unit for you, the consumer. **It won't work with 1-2-4 style room thermostats like the TR200**
Again, when in doubt, ask a technician.
3) Awareness to short circuits and bus failures due to wrong wiring
4) Direct access to the heating itself in case of problems.
5) No, really, you shouldn't mess with things that aren't **yours**
6) Ideally an ESP32 Kit but if you are just interested in the CAN-Stuff you can of course throw away all the MQTT and WiFi and just use a bog standard arduino.
7) A MCP2515 + TJA1050 Can-Bus module (i.e. branded "NiRen")
8) A MQTT broker (i.e. Mosquitto)
9) Visual Studio Code & Platform.IO Add-On are recommended!
10) Optional: DS18B20 Sensors

## Features

### MQTT
Have values where you need them, control on demand. You are able to actively steer the heating towards certain temperatures or modes of operation by publishing and subscribing to MQTT topics from within your favorite MQTT broker (Mosquitto is recommended).
The topics are described inside `/data/configuration.json`

To send parameters to the heating controller, you just have to form a JSON and send it to the topic you defined in `/data/configuration.json`

The `MQTT` section has everything and this is where you define the topics:

```json
    "MQTT": {
        "Server": "1.2.3.4",
        "Port": 1883,
        "User": "mqtt",
        "Password": "mqtt",
        "Topics": {
            // Topic for receiving temperatures and status
            "HeatingValues": "cerasmarter/heating/values",
            // Send values here to steer the heating circuit and functions
            "HeatingParameters": "cerasmarter/heating/parameters",
            // Topic for receiving temperatures and status
            "WaterValues": "cerasmarter/water/values",
            // Send values here to steer the hot water circuit and functions
            "WaterParameters": "cerasmarter/water/parameters",
            // Topic for receiving temperatures from auxilary sensors
            "AuxilaryParameters": "cerasmarter/auxilary/values",
            // Topic for receiving status information
            "Status": "cerasmarter/status",
            // Send values here to receive values on demand
            "StatusRequest": "cerasmarter/status/get"
        }
    },
```

#### Example Parameter JSON for setting Heating Parameters:
```json
{
  //Enable the heating mode
  "Enabled": false,
  //Setpoint for Feed Temperature
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
  "ValveScaling": 1,
  "ValveScalingMaxOpening": 100,
  "ValveScalingOpening": 75,
  "DynamicAdaption": 1,
  "OverrideSetpoint": false
}
```

### Heating Parameters
Originally the TAXXX and integrated Heatronic will follow a set of parameters to determine the right feed temperature according to outside temperatures. These values are commonly referred to as "base point" and "end point" and represent a linear regulation by a reference temperature - the environmental temperature on the outside.
The original controller will take the desired minimum feed temperature at -15°C as the end point and the required feed temperature at 20°C as the base point.
**The meaning of base and end point is turned around in this project!**
Why is that so?
Because we are now looking at the environment temperature(s) and we know what the heating can deliver it is easier to understand what we want to achieve.
The base point now represents the outside temperature at which the heating should use the maximum possible feed temperature as dialed in by the heating circuit dial on the heating itself
The end point is basically the temperature at which the heating should switch off.
![Linear distibution](/assets/Temperature_Mapping_Explained.jpg)
*In this graph the base point is -10°C and the end point is 20°C meaning at -10°C we need the full power to keep our home warm whereas 20°C is when we don't need it anymore*

See `FeedBaseSetpoint` for base point, `FeedCutOff` for end point or "cut off" temperature 
[See Example JSON](#example-parameter-json-for-setting-heating-parameters)

### Night/Economy Mode
There are two ways to switch the economy mode.
#### Option #1 (MQTT):
 Set `Enabled` to `false` or `0` using the Parameters JSON file which you send to the Heating Parameters Topic defined in `/data/configuration.json`

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
"Enabled": true,
```

#### Option #2 (Amend Code):
 Set `commandedValues.Heating.Active` to `false` or `true` depending on if you want to switch economy on or off.

### Switch Off/On

See [Night/Economy Mode](#nighteconomy-mode)

Hint: The manufacturer recommends to not turn the heating off by the power switch but instead set it into economy mode with 10° feed temperature (lowest setting) to prevent getting the pump or valves stuck. If set to economy the heating will move the pump(s) and valve(s) every 24h if they haven't been moved within that range.

### Boost
Boost function sets the feed temperature to the maximum reported value (`ceraValues.Heating.FeedMaximum`) for a selected period of time (default: 300 seconds). This is especially useful when you own electronic or "smart home" thermostats in general which in most cases offer such a boost function. the problem with this "boost" is that although the valve opens up for a few minutes, the heating won't actually deliver the required temperature. A common misunderstanding is that opening the valve to the highest setting will heat more. It will instead only *allow* for a much higher room temperature as the water flow through the system is almost unchanged.
Due to the natural lag of a heating system you should fire this function before you boost a specific radiator.

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
  "OnDemandBoost": false,
  "OnDemandBoostDuration": 600,
```

### Fast Heatup
Fast Heatup function compares a temperature (`commandedValues.Heating.AmbientTemperature`) to a given target value (`commandedValues.Heating.TargetAmbientTemperature`) and sets the feed temperature to maximum (`ceraValues.Heating.FeedMaximum`) as long as the temperature hasn't reached the target value. It will slowly decrease the feed temperature down from maximum as the target is approached. 
![Fast Heatup Demo](/assets/fastheatup_demo.jpg)
*This is how the fast heatup function works visually*

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
  "FastHeatup": false,
```
*set to `true` to enable this feature*

### Fallback and Failsafe

The parameters defined within `heating.h` will become active when the connection to the MQTT broker has been lost.

```c++
  //-- Fallback Values
  //TODO: Should be configurable using configuration.json
  struct FallBack_
  {
    //-- Basepoint Temperature
    double BasepointTemperature = -10.0F;
    //-- Endpoint Temperature
    double EndpointTemperature = 31.0F;
    //-- Ambient Temperature
    double AmbientTemperature = 17.0F;
    //-- Minimum ("Anti Freeze") Temperature.
    double MinimumFeedTemperature = 10.0F;
    //-- Enforces the fallback values when set to TRUE
    bool isOnFallback = false;

    //-- Heating Scheduler. Fallback values for when the MQTT broker isn't available
    HeatingScheduleEntry fallbackStartEntry = {5, 30, 0, true};
    HeatingScheduleEntry fallbackEndEntry = {23, 30, 0, false};
  } Fallback;
```

The `HeatingScheduleEntry` represents a very basic timeslot:
Trigger Hour, Trigger Minute, Day Of Week, Heating Enabled.
`Day Of Week` is currently unused.

The aforementioned values say: 
- Turn on the heating every day at 5:30
- Turn off the heating every day at 23:30


### Automatic Controller Detection

Other controllers on the network will send their messages which always start at ID `0x250`. As soon as such a message is detected, the `Override` flag will turn to `false` and our controller will stop sending control messages. If there is no controller message on the network for 30 seconds (defined by `BusMessageTimeOut` within `/data/configuration.json`) it will resume control and the `Override` flag returns to true.

Example:
```json
    "General": {
        "BusMessageTimeout": 30,
        "Debug": false,
        "Sniffing": true
    },
```
*Timeout is set to 30 seconds*


You could implement this as a solution to bring in the original controller when something isn't working as expected and you don't have direct access to the ESP. You could switch back on/plug in the original TAxxx unit to run it in OEM mode.
Maybe you switch on a relay that triggers the voltage supply for the original controller or you instruct someone to plug the TAxxx unit back in.


### External Temperature Sensors

The oneWire and DallasTemperature libraries are included and used to fetch additional temperatures like the return temperature which isn't available on the bus.

#### Configuration

Configured using `/data/configuration.json`
**IMPORTANT**
The `Count` has to be less or equal the amount of sensor you have defined under `Sensors` or else the program will crash!

Each sensor is defined by a label, if it's used to reference the return temperature and its address.
In the following example the sensor is called "Feed" (`"Label": "Feed"`) and shouldn't be used as a reference temperature (`"IsReturnValue": false`) in [Dynamic Adaption](#dynamic-adaption) Mode.
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

### Dynamic Adaption

Simply put: my heating system is way too powerful and the radiators are not capable of getting rid of the energy fast enough. I figured if I lower the temperature by the difference between return and desired room temperature I can get away with a more dynamic model:

Adaption = Desired target room temperature - feed temperature (+ Manual Adaption)

This means that the setpoint of the feed will be lowered by the difference. You don't have to pump in that much energy when 90% of it returns to the heat exchanger which by itself will always try to steer the feed temperature to the average setpoint.

This kind of adaption is, of course, very simple and rough. As soon as this mode is active, the `Adaption` value will alter the value accordingly so in my instance I put in additional 5° `Adaption` so if the temperature would be lowered to around 35° feed, it will be actually set to 40°.

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
  "Adaption": 0
```

### Calculate yourself

You can do your own calculations and just tell the control to set the temperature accordingly by setting `1|0` or `true|false` via parameters topic to enable or disable this feature and set the desired `FeedSetpoint`

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
  [...]
  "FeedSetpoint": 70,
  [...]
  "OverrideSetpoint": true
```

### Valve-based control

This feature will calculate the desired feed temperature based on the valve opening that is received, mapped using the minimum a valve can be closed (0%) and the maximum (80% for Homematic eTRV-2, set by `ValveScalingMaxOpening`) to the `FeedMinimum` plus `Adaption` and `FeedMaximum` (Reported on the topic defined at `HeatingValues` within [Configuration](data/configuration.json))
This is the most demand focused function yet because if you always report the most open valve in the circuit, you end up with a very responsive system that will react on demand immediately.
This also means that, for example, in the morning when the heat cycle starts, the most open valve will most likely report it is running at the maximum available opening thus raising the feed setpoint to max. As the rooms get warmer and warmer it will eventually throttle and another valve may be higher. This is a self regulating system which will deactivate the influence of outside temperatures. If you want to have the temperature influenced by outside temperature, switch on `DynamicAdaption` which will then add a value mapped between `FeedBaseSetpoint` and `FeedCutoff`. Next set `Adaption` according to your needs.

Example for dynamic adaption together with valve scaling:
- Outside Temperature is 5°
- Basepoint is -15°
- Endpoint is 31°
- Feed Adaption is 20
- Valve Opening: 50%
- Max Feed: 75°


Map Function: `(x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min`

Adaption Calculation: (Outside - Basepoint) * (Feed Adaption - 0) / (Endpoint - Basepoint) + 0

Example: `(5 - -15) * (20 - 0) / (31 - -15) + 0 = 8.7`

Feed Calculation: (Valve Opening - Min-Valve Opening) * (Max Feed - (Min-Feed + Adapt. Calc.)) / (Max. Valve - Min. Valve) + (Min. Feed + Adapt. Calc.)

Example: `(50 - 0) * (75 - (10 + 8.7)) / (80 - 0) + (10 + 8.7) = 53.89`

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
{
[...]
  //Enable valve scaling
  "ValveScaling": true,
  //The maximum value of a valve
  "ValveScalingMaxOpening": 100,
  //The current valve opening
  "ValveScalingOpening": 75,
[...]
}
```

### OTA Updates and Console

The standard "Arduino OTA" procedure is included which means you can upload the code to your ESP32 without having to plug in the USB cable. See `platformio.ini` and modify the IP address accordingly.

Debug info can be retrieved using a very basic telnet implementation. Simply connect to the ESP32 using telnet and watch as the messages flow. You can reboot the ESP by typing `reboot` and press enter. Be aware you have to type very quickly because this is truly a very minimalistic and barebone implementation of a client-server console communication which is primarily designed to see debug output without having to stand near the esp.

## Hints
- If you just wanna read then you have to set the variable `Override` to `false`. This way nothing will be sent on the bus but you can read everything.
- For debug purposes the `Debug` variable controls wether you want to see verbose output of the underlying routines like feed temperature calculation and step chain progress.
- Keep in mind that if you are intending to migrate this to an arduino you have to watch out for the OTA feature and `float` (`%f`) format parameters within `sprintf` calls.
- When OTA is triggered, all connections will be terminated except the one used for OTA because otherwise the update will fail. The MC will keep working.
- The OTA feature is confirmed working with Arduino IDE and Platform.io but for the latter you have to adapt the settings inside `platformio.ini` to your preference.

## Configuration / Setup
1) See `configuration.json` inside the `data` folder to configure the project to your requirements.
2) Upload the project
3) Now It will format the SPIFFS filesystem.
4) `Build Filesystem Image` and `Upload Filesystem Image` so the configuration is stored on SPIFFS
5) It will now read this config and will reach operational status

You should be able to perform OTA updates now for both application and configuration.

## Dedicated PCB

WIP

## Todo
- [x] Find a suitable CAN module and library that is able to handle 10kbit/s using the ESP32
- [x] Debug output over Telnet
- [x] OTA Update Capability
- [x] Try not to get mad while searching for the reason why the OTA update is aborting at around 2-8%
- [x] Collecting IDs and their meaning
- [x] Getting the calculations right for the feed setpoint
- [x] Reading and writing MQTT topics
- [x] Fallback Mode
- [ ] Taking Weather conditions into account when calculating the feed temperature
- [x] Also taking indoor temperatures into account
- [x] Getting the timings right so it doesn't throw off the controller
- [x] Testing as a standalone solution
- [x] Example Configuration for Home Assistant
- [ ] Dedicated PCB with all components in place and power supply through the controller

## Special Thanks
- The people at the mikrocontroller.net forums
- Pierre Molinaro and contributors of the ACAN2515 library: https://github.com/pierremolinaro/acan2515
- Nick O'Leary and contributors of the PubSubClient library: https://github.com/knolleary/pubsubclient
- Rop Gonggrijp and contributors of the ezTime library: https://github.com/ropg/ezTime
