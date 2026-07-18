# Cerasmarter
[![CI](https://github.com/Neuroquila-n8fall/JunkersControl/actions/workflows/build-master.yml/badge.svg)](https://github.com/Neuroquila-n8fall/JunkersControl/actions/workflows/build-master.yml)
## NOTE: Documentation is mostly accurate right now.
**Feel free to open an issue if something is unclear.**
## Table of contents
- [Cerasmarter](#cerasmarter)
  - [NOTE: Documentation is mostly accurate right now.](#note-documentation-is-mostly-accurate-right-now)
  - [Table of contents](#table-of-contents)
  - [Community](#community)
  - [Purpose and Aim](#purpose-and-aim)
    - [Cerasmart-er](#cerasmart-er)
  - [Contribution](#contribution)
  - [Intended Audience](#intended-audience)
    - [Disclaimer:](#disclaimer)
  - [A word of warning](#a-word-of-warning)
    - [But why? We are talking about a data line!](#but-why-we-are-talking-about-a-data-line)
  - [Prerequisites](#prerequisites)
  - [Installation (Quick Start)](#installation-quick-start)
  - [Features](#features)
    - [MQTT](#mqtt)
      - [Where?](#where)
      - [How?](#how)
      - [Example Parameter JSON for setting Heating Parameters:](#example-parameter-json-for-setting-heating-parameters)
      - [Examples & Detailed Explanation](#examples--detailed-explanation)
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
      - [Where?](#where-1)
    - [Dynamic Adaption](#dynamic-adaption)
    - [Calculate yourself](#calculate-yourself)
    - [Valve-based control](#valve-based-control)
  - [Updating](#updating)
  - [Telnet Console](#telnet-console)
  - [Home Assistant Integration](#home-assistant-integration)
  - [Hints](#hints)
  - [Getting Started](#getting-started)
    - [Configuration](#configuration)
  - [Dedicated PCB](#dedicated-pcb)
  - [Todo](#todo)
  - [Special Thanks](#special-thanks)

![Alt_Text](/assets/example_ha_dashboard.jpg)

## Community
You can reach out to us on [Discord](https://discord.gg/9Wrndbqu7t) where we can discuss and help eachother.

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
Since the upgrade to a flexible configuration format and the ability to configure things via a web UI, you will be able to tune the system to your needs without knowing much about software development.
But you might find yourself in a situation where things don't work as expected. Feel free to open an issue so we can adapt things to your needs.

### Disclaimer:
I have sourced the message ids from https://www.mikrocontroller.net/topic/81265 but only process those that are relevant to me.
This means you should bring a little bit of patience and basic knowledge about your heating with you so you can actually make decisions about what IDs might be relevant to you or not.
You should look at the CAN-Bus configuration on the web UI and the Can Analyzer to sort things out for your heating system.
Also you should have Visual Studio Code ready and the extension [Platformio](https://platformio.org/) installed. We'll talk about that in detail in [Prerequisites](#prerequisites)


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
7) A MCP2515 + TJA1050 Can-Bus module (i.e. branded "NiRen"). Other boards with different controllers and transceivers may work too.
8) A MQTT broker (i.e. Mosquitto)
9) Visual Studio Code & [Platform.IO](https://platformio.org/) Add-On are recommended!
10) Optional: DS18B20 Sensors

## Installation (Quick Start)
Either clone the repository and build/upload yourself using Platformio and your IDE of choice or download the binaries and use esptool as follows:
Example for Windows environments:
```shell
esptool.exe --after no_reset --chip esp32 --baud 921600 --port <Serial port of your device> write_flash --verify 0x10000 firmware.bin
esptool.exe --after hard_reset --chip esp32 --baud 921600 --port <serial port of your device> write_flash 0x307000 littlefs.bin
```

The important part is the addresses `0x10000` for the firmware and `0x307000`for the filesystem.

Release filesystem images contain the credential-free configuration template as `/configuration.json`; they never contain a developer's device-specific configuration. Its empty WiFi credentials make a new controller start the `CERASMARTER` access point for provisioning through the web UI.

If everything went well you should see the following output on the console:
```log
Press the "BOOT" button within the next 5 seconds to enable Setup Mode!
Setup Mode not enabled. You can enable it at every time by pressing the "BOOT" button once.
Invalid WiFi configuration. Launching AP mode.
WiFi AP launched. Find me @ 192.168.4.1
```

Startup reports filesystem problems separately:

- **LittleFS could not be mounted or formatted** indicates a flash-partition or hardware problem.
- **LittleFS was formatted and is empty** means the partition recovered but `littlefs.bin` must be uploaded again.
- **Web frontend is missing** means LittleFS exists, but the filesystem image is absent or incomplete.
- **Configuration is missing or invalid** starts the setup access point when the frontend is available, so the configuration can be repaired through the web UI.

Now you can connect to the AP ("CERASMARTER" network by default) and modify/import your configuration. A sample configuration is located [here](assets/Templates/Configurations/configuration.json). After uploading `/configuration.json` in the file manager, use **Reload Configuration** to validate and activate it without a power cycle.

If MDNS is working properly on your end, you will be able to open the web UI using http://cerasmarter/

### Web interface preferences

The navigation bar provides English and German language selection and a sun/moon light/dark appearance toggle. Both preferences are stored in the current browser and apply to every web-interface page. On first use, the browser language selects German when appropriate, and the operating-system color preference selects the initial appearance.

Translations are maintained as one JSON resource per locale in `data/frontend/i18n/` (`en.json`, `de.json`). Keys are grouped by their UI area, for example `menu.home`, `dashboard.system_status`, `mqtt.prefix`, and `filemanager.reload`. Markup can opt into automatic translation with `data-i18n="area.key"`; translated attributes use `data-i18n-placeholder`, `data-i18n-title`, or `data-i18n-aria-label`. JavaScript uses `translate("area.key", values)`. English is the fallback locale, and new locales only require a new resource plus an entry in `UiLocales`.

The CAN Message Analyzer under **Utilities** reads the active CAN address configuration and displays names such as `Heating · Feed Current` beside their hexadecimal IDs. This makes captures usable with customized address maps while unknown IDs remain clearly identified.

The home page is a live heating dashboard. It shows burner, feed, outside, and hot-water values plus a lightweight five-minute temperature chart that runs entirely in the browser without an external chart service. **Utilities > Fallback Heating Control** exposes every runtime command accepted through MQTT, including heating-curve, room-reference, boost, fast-heatup, valve-scaling, and hot-water controls. These commands take effect immediately, refresh the fail-safe command lease, and are deliberately not written to `configuration.json`; MQTT or Home Assistant can replace them later.

The dashboard data is also available as JSON from `GET /api/runtime`. Runtime commands can be sent as a partial JSON object to `POST /api/control`, using the same field names as the heating MQTT payload plus `Boost`, `FastHeatup`, and `HotWaterSetpoint`. Both transports use the same firmware command handlers.

## Features

### MQTT
Have values where you need them, control on demand. You are able to actively steer the heating towards certain temperatures or modes of operation by publishing and subscribing to MQTT topics from within your favorite MQTT broker (Mosquitto is recommended).

#### Where?
The topics are described inside `/configuration.json` on the device or under `Configuration > MQTT` in the web UI.

#### How?

To send parameters to the heating controller, you just have to form a JSON and send it to the topic you defined in `/configuration.json`.

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
            // Topic for receiving temperatures from auxiliary sensors
            "AuxiliaryValues": "cerasmarter/auxiliary/values",
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
  "AuxiliaryTemperature": 11.6,
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

#### Examples & Detailed Explanation
- See [MQTT Message Exchange: Receive](assets/Examples/MQTT_Message_Exchange/Receive/README.md)
- See [MQTT Message Exchange: Send](assets/Examples/MQTT_Message_Exchange/Send/README.md)

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
 Set `Enabled` to `false` or `0` using the Parameters JSON file which you send to the Heating Parameters Topic defined in `/configuration.json`.

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

The controller starts in local fail-safe mode after every boot and leaves it only after a recognized heating command arrives. That command refreshes the `CommandTimeoutSeconds` lease; broker reconnection alone does not leave fail-safe mode.

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

On entry, boost, fixed-setpoint override, fast heatup, valve scaling, dynamic adaptation, and manual adaptation are cleared. Between the daily start and stop times the configured heating curve is used and capped at `MaximumFeedTemperature`. Outside the window heating is disabled. If neither boiler time nor synchronized local time is available, `HeatWhenTimeUnknown` selects between minimum-temperature heating and heating disabled.

WiFi, MQTT, and NTP recovery are cooperative and bounded; they do not own or suspend the CAN control loop.


### Automatic Controller Detection

Other controllers on the network will send their messages which always start at ID `0x250`. As soon as such a message is detected, the `Override` flag will turn to `false` and our controller will stop sending control messages. If there is no controller message on the network for 30 seconds (defined by `BusMessageTimeOut` within `/configuration.json`) it will resume control and the `Override` flag returns to true.

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

#### Where?

Configured using `/configuration.json` on the device or `Configuration > Temperature Sensors` in the web UI.

See [Auxiliary Sensors](assets/Configuration.md#auxiliary-sensors)

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

This feature will calculate the desired feed temperature based on the valve opening that is received, mapped using the minimum a valve can be closed (0%) and the maximum (80% for Homematic eTRV-2, set by `ValveScalingMaxOpening`) to the `FeedMinimum` plus `Adaption` and `FeedMaximum` (reported on the topic defined at `HeatingValues` within the [configuration template](assets/Templates/Configurations/configuration.json)).
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

## Updating

The standard "Arduino OTA" procedure is included which means you can upload the code to your ESP32 without having to plug in the USB cable. See `platformio.ini` and modify the IP address accordingly.

You can also use the web UI (See: Firmware Update on the menu bar) to upload the `firmware.bin` and `littlefs.bin` files to update the firmware and filesystem image.

- A firmware-only update preserves the existing LittleFS configuration.
- A `littlefs.bin` update performed through the web interface validates and copies the active configuration to the separate NVS partition before replacing LittleFS. On the next boot it restores the configuration once, before normal configuration loading. The update is cancelled if a safe backup cannot be made.
- Keep a downloaded `/configuration.json` backup for recovery. Direct PlatformIO, esptool, or programmer-based filesystem flashing cannot be intercepted by the running firmware and therefore still installs the credential-free template. After such an update, upload the backup as `/configuration.json` and click **Reload Configuration**.
- Configuration saves from the web UI are written atomically and verified before they replace the previous file. If validation or writing fails, the previous configuration remains available as a backup.

## Telnet Console

Debug info can be retrieved using a very basic telnet implementation. Simply connect to the ESP32 using telnet and watch as the messages flow. You can reboot the ESP by typing `reboot` and press enter. Be aware you have to type very quickly because this is truly a very minimalistic and barebone implementation of a client-server console communication which is primarily designed to see debug output without having to stand near the esp.

## Home Assistant Integration

Cerasmarter uses current MQTT **device discovery**. No Home Assistant YAML files or separate discovery templates are required.

Set `HomeAssistant.Enabled` to `true`, configure the same discovery prefix used by Home Assistant (normally `homeassistant`), and give every controller a unique `DeviceId`. After connecting to MQTT, the controller publishes one retained device-discovery document to:

```text
<AutoDiscoveryPrefix>/device/<DeviceId>/config
```

Home Assistant groups the heating, hot-water, status, and auxiliary-temperature entities under one device. The integration provides temperature and status sensors, binary operating-state sensors, dynamically generated auxiliary-sensor entities, number controls for requested feed temperature, boost duration, and room-reference temperature, plus switches for heating enablement, boost, and fast heatup.

Memory, filesystem and flash capacity, chip model and revision, CPU core count, CPU frequency, and auxiliary-sensor connectivity are exposed as diagnostic entities. Home Assistant keeps these on the device's diagnostic card instead of treating them as normal heating controls or measurements.

Discovery assigns purpose-specific Material Design icons to every entity. The **Flame Lit** binary sensor intentionally has no Home Assistant device class: it therefore reports plain **On/Off** while using a flame icon whose active color follows the burner state. MQTT discovery supports one static icon per entity, so using different custom glyphs for its on and off states would require a separate Home Assistant template entity.

Runtime state and command topics use `cerasmarter/<DeviceId>/...`, independently of the discovery prefix. Discovery and state messages are retained, and MQTT Last Will availability marks the device offline if its connection is lost. The controller also listens for Home Assistant's `<AutoDiscoveryPrefix>/status` birth message and republishes discovery when Home Assistant restarts.

Keep `DeviceId` stable after discovery. If it or the discovery prefix is changed through the web interface, the controller removes its previous retained discovery record and reconnects with the new identity. An old record only needs manual removal if the broker was unavailable during the change.

See the [configuration guide](assets/Configuration.md#home-assistant) for all settings.

## Hints
- If you just wanna read then usually you have nothing to modify. The program will see other controllers on the bus and will go into "read-only" mode by itself. If you're not wanting to take any risks, you have to set the variable `OverrideControl` in [main.cpp](src/main.cpp) to `false`. This way nothing will be sent on the bus udner any circumstances but you can read everything.
- For debug purposes the `configuration.General.Debug` variable controls wether you want to see verbose output of the underlying routines like feed temperature calculation and step chain progress.
- Keep in mind that if you are intending to migrate this to an arduino you have to watch out for the OTA feature and `float` (`%f`) format parameters within `sprintf` calls.
- When OTA is triggered, all connections will be terminated except the one used for OTA because otherwise the update will fail. The MC will keep working.
- The OTA feature is confirmed working with Arduino IDE and Platform.io but for the latter you have to adapt the settings inside `platformio.ini` to your preference.

## Getting Started

1. Build and upload the firmware and LittleFS image, or install both binaries from a GitHub release.
2. Connect to the `CERASMARTER` access point and open `http://192.168.4.1/` if no valid WiFi configuration exists yet.
3. Configure the device through the web UI, or customize the [configuration template](assets/Templates/Configurations/configuration.json) and upload it as `/configuration.json` in the file manager.
4. Click **Reload Configuration** after uploading a complete file. The file is validated before it becomes active.
5. Reconnect using the configured hostname or IP address after WiFi starts.

Do not place a real `configuration.json` in the repository's `data` directory when preparing releases. The filesystem build always copies the credential-free file from `assets/Templates/Configurations/configuration.json`; a device-specific file could contain WiFi and MQTT credentials.

For local USB provisioning only, set `CERASMARTER_CONFIG_FILE` to an external configuration file while running the `buildfs`/`uploadfs` targets. This stages that file in the generated image without adding it to the repository. The GitHub release workflow never sets this override and verifies that release images use the credential-free template.

### Configuration

See the [Configuration](assets/Configuration.md) guide for details.

## Dedicated PCB

WIP

**Update 09-2022**: The bus is very picky about the choice of hardware. Multiple prototypes have been built and tested and further investigation is in progress. Another limitation is the amount of current the built-in power supply can deliver. This is slowing down hardware development even more.

## Todo
- [x] Find a suitable CAN module and library that is able to handle 10kbit/s using the ESP32
- [x] Debug output over Telnet
- [x] OTA Update Capability
- [x] Try not to get mad while searching for the reason why the OTA update is aborting at around 2-8%
- [x] Collecting IDs and their meaning
- [x] Getting the calculations right for the feed setpoint
- [x] Reading and writing MQTT topics
- [x] Configurable offline fail-safe
- [ ] Taking Weather conditions into account when calculating the feed temperature
- [x] Also taking indoor temperatures into account
- [x] Getting the timings right so it doesn't throw off the controller
- [x] Testing as a standalone solution
- [x] Example Configuration for Home Assistant
- [ ] Dedicated PCB with all components in place and power supply through the controller
- [ ] Take more intelligent decisions for feed temperatures
- [ ] Restructure code into reusable classes

## Special Thanks
- The people at the mikrocontroller.net forums
- Pierre Molinaro and contributors of the ACAN2515 library: https://github.com/pierremolinaro/acan2515
- Nick O'Leary and contributors of the PubSubClient library: https://github.com/knolleary/pubsubclient
- Rop Gonggrijp and contributors of the ezTime library: https://github.com/ropg/ezTime
- The maintainers of the ArduinoJSON library: https://arduinojson.org/
- The maintainers of the Async ESP Webserver and AsyncTCP library: https://github.com/me-no-dev/ESPAsyncWebServer
