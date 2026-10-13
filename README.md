# Cerasmarter
[![CI](https://github.com/Neuroquila-n8fall/JunkersControl/actions/workflows/build-master.yml/badge.svg)](https://github.com/Neuroquila-n8fall/JunkersControl/actions/workflows/build-master.yml)

## Documentation

- [Installation and feature overview](README.md#installation-quick-start)
- [Configuration reference](assets/Configuration.md)
- [Web, SSE, and MQTT API reference](assets/API.md)
- [CAN Analyzer guide](assets/CANAnalyzer.md)
- [MQTT command examples](assets/Examples/MQTT_Message_Exchange/Receive/README.md)
- [MQTT state examples](assets/Examples/MQTT_Message_Exchange/Send/README.md)

Documentation tracks the current development firmware. Please open an issue when controller-specific behavior differs from the documented BM1 installation profiles.

**Beta release: `0.95.0` (2026-10-13).** Adds bounded dynamic-valve weather support, optional forecast assistance and updated ESP32 packaging. Forecast assistance is disabled by default. Read the [English release notes](assets/Releases/0.95.0.md) for installation assets, configuration backups, trusted-LAN requirements and verification limits before upgrading.

## Community
You can reach out on [Discord](https://discord.gg/9Wrndbqu7t) to discuss the project and get help from the community.

## Purpose and Aim
This project is designed around the idea of having a SCADA-like setup where your command & control server (MQTT-Broker) sends commands and receives the status of the heating.
Since the rise of modern and affordable "Smart Radiator Thermostats" we are able to precisely control the room temperature whereas the usual central heating system is only able to react on outside temperatures and doesn't know what the actual demand is like. The heating is only capable of reacting to certain drops in feed and return temperatures and heat up according to setpoints defined by the consumer. This principle is proven and hasn't changed until today.

### Cerasmart-er
The Junkers/Bosch "Heatronic" controller of the "Cerasmart" type of boilers (yes they used that term back in pre-2000!) is by itself smart enough to keep your home warm without wasting too much energy, if properly equipped and configured, of course. You won't receive any benefits from this project if you are messing around with the parameters without knowing the concepts of a central heating system. Also if your heating isn't "tuned" to your home it will waste money nonetheless.
**This also means you can't tell the boiler to perform unreasonable actions because the controller inside the boiler unit is in charge of controlling the actual temperatures within a safe, predefined range that has been set either by the manufacturer or the technician that maintains your heating.**
We are, however, able to suggest certain temperatures or switch the boiler on and off.

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

1) A compatible Bosch-Junkers central gas heating system with Bosch Heatronic controller and a BM1 Bus Module. BM2 installations are intentionally outside this project's scope.
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
Either clone the repository and build/upload it with PlatformIO, or download the release binaries. For a new board, a board that still contains factory firmware, or a board with an unknown partition table, flash the complete image at address `0x0`:

```shell
esptool.exe --after hard_reset --chip esp32 --baud 921600 --port <serial port of your device> write_flash --verify 0x0 cerasmarter-complete.bin
```

The complete image contains the bootloader, Cerasmarter partition table, OTA metadata, firmware, and LittleFS filesystem. Flashing only `firmware.bin` at `0x10000` does **not** install the required partition table and will not boot on a board carrying an incompatible factory layout.

After the complete image has provisioned the board once, the component images can be used for updates while retaining the other flash regions:

```shell
esptool.exe --after hard_reset --chip esp32 --baud 921600 --port <serial port of your device> write_flash --verify 0x10000 firmware.bin
esptool.exe --after hard_reset --chip esp32 --baud 921600 --port <serial port of your device> write_flash --verify 0x307000 littlefs.bin
```

These component offsets are valid only with the matching `partitions_custom.csv` layout. The complete image is intended for first installation or deliberate full recovery; reflashing it also replaces the configuration stored in both NVS and LittleFS.

Release filesystem images contain the credential-free configuration template as `/configuration.json`; they never contain a developer's device-specific configuration. Its empty WiFi credentials make a new controller start the `CERASMARTER` access point for provisioning through the web UI.

If everything went well you should see the following output on the console:
```log
Press BOOT within 5 seconds for Setup Mode; hold it for 10 seconds to factory-reset configuration.
Setup Mode not enabled. You can enable it at every time by pressing the "BOOT" button once.
Invalid WiFi configuration. Launching AP mode.
WiFi AP launched. Find me @ 192.168.4.1
```

A short BOOT-button press during the startup window enters Setup Mode without changing configuration. Holding BOOT continuously for 10 seconds performs a deliberate factory reset: it clears the persistent NVS mirror and all LittleFS configuration copies, then starts the provisioning access point. The frontend and other filesystem assets are retained.

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

The CAN Message Analyzer under **Utilities** receives live bus events independently of verbose console sniffing. It groups traffic by ID, displays configured value names, frame counts, DLCs, mean intervals, last payloads, byte deltas, and common candidate interpretations. Captures can be paused, filtered, limited on screen, and exported as JSON or CSV for comparison. Unknown IDs remain clearly identified. See the [CAN Message Analyzer guide](assets/CANAnalyzer.md) for a safe capture workflow, field descriptions, interpretation guidance, and troubleshooting.

![Cerasmarter web dashboard](/assets/webui-dashboard-dark.png)

_Production-controller capture. The displayed `Burner power` label was renamed to `Feed temperature utilization` in the current development UI because it represents the feed-temperature range, not thermal burner output._

The home page is a live heating dashboard organized into heating, mixed-circuit, domestic-hot-water, auxiliary-sensor, runtime-control, and system categories. It presents the main operational values from `GET /api/runtime`, uses equipment-appropriate state icons, and plots a five-minute temperature history with the locally bundled Chart.js 4.5.1 library. No CDN or internet access is required. The heating card includes a localized weather/forecast monitor: maximum weather fraction, forecast enablement, a two-decimal coefficient, available correction, sample lifetime, reduction permission and raw forecast trend. Sample-dependent values display a dash when no fresh sample exists; available correction is not confirmation that it changes the transmitted request. **Utilities > Fallback Heating Control** exposes heating-curve, room-reference, boost, fast-heatup, valve-scaling, weather/forecast settings and hot-water controls. These web fallback commands are queued for the main loop through `POST /api/control/queued` and are not persisted; stable settings received through MQTT or Home Assistant are stored in `RuntimeControls` and restored after reboot. Forecast samples arrive separately through the HTTP/MQTT interfaces.

At the end of the home page, **System Status** displays the running firmware version beside the MCU model. Its value comes from `GET /api/info`, not the website build; an absent version field is shown as `—`.

![Fallback heating controls](/assets/webui-fallback-control-dark.png)

The dashboard data is also available as JSON from `GET /api/runtime`. Runtime commands can be sent as a partial JSON object to `POST /api/control`, using the same field names as the heating MQTT payload plus `Boost`, `FastHeatup`, and `HotWaterSetpoint`. Both transports use the same firmware command handlers. See the [API reference](assets/API.md) for request schemas, ranges, persistence behavior, the CAN event stream, and security limitations.

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
            "HeatingValues": "cerasmarter/heating/values",
            "HeatingParameters": "cerasmarter/heating/parameters",
            "WaterValues": "cerasmarter/water/values",
            "WaterParameters": "cerasmarter/water/parameters",
            "AuxiliaryValues": "cerasmarter/auxiliary/values",
            "Status": "cerasmarter/status",
            "StatusRequest": "cerasmarter/status/get",
            "Boost": "cerasmarter/boost/set",
            "FastHeatup": "cerasmarter/fastheatup/set"
        }
    },
```

#### Example Parameter JSON for setting Heating Parameters:
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
Set `Enabled` to `false` using the JSON document sent to the configured Heating Parameters topic, the Home Assistant **Heating Enabled** switch, or **Utilities > Fallback Heating Control**.

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
"Enabled": true,
```

### Switch Off/On

See [Night/Economy Mode](#nighteconomy-mode)

The manufacturer-defined 10 °C (50 °F) effective feed setpoint represents an off request. Every calculated result below that minimum is normalized to exactly 10 °C before shutdown evaluation, CAN conversion, or transmission. Cerasmarter then switches the heating operation flag off if the effective setpoint remains at 10 °C for `General.SetpointOffDelaySeconds`. The default grace period is 300 seconds, avoiding short demand changes that could repeatedly cycle the igniter, valves, or pump.

This guard is part of the controller core rather than the Home Assistant integration. It applies equally to MQTT, Home Assistant, WebUI, and locally calculated setpoints. Once active, repeated `Enabled: true` commands cannot override the off state while the effective feed request remains at 10 °C. A higher effective setpoint clears the guard; clients that explicitly manage `Enabled` should send it as `true` with the new demand.

Temperatures remain Celsius internally. Home Assistant performs the equivalent Fahrenheit display conversion when the installation uses imperial units.

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

The controller starts in local fail-safe mode after every boot and leaves it as soon as MQTT connects. No periodic command heartbeat is required, and the controller does not assume that Node-RED or any other particular automation system is present.

For configuration-file compatibility, the setting remains named `CommandTimeoutSeconds`. It now defines how long MQTT may remain continuously disconnected before fail-safe mode starts. When that grace period expires, the controller saves the current runtime controls and applies the local fail-safe profile. Reconnecting to MQTT leaves fail-safe mode and restores those saved controls; command frequency does not affect activation.

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

The controller starts in read-only mode and watches CAN IDs `0x250` through `0x25F` for another room controller. As soon as such a message is detected, the `Override` flag remains `false` and Cerasmarter does not send control messages. Only after a complete interval with no controller traffic (30 seconds by default, configured by `BusMessageTimeout` in `/configuration.json`) does it assume control and set `Override` to `true`.

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

With valve scaling enabled, dynamic adaptation selects the outside-temperature baseline described in [Valve-based control](#valve-based-control). Outside temperature therefore supports heat delivery even when `Adaption` is zero. A positive manual adaptation raises that baseline more in cold weather and fades toward the warm cutoff; its direction is no longer reversed.

Without valve scaling, dynamic adaptation retains the existing correction `TargetAmbientTemperature - FeedReturnTemperatureReference + Adaption`. This is a return-reference correction, not a measurement of radiator output or building heat loss. Fast heatup temporarily uses its own temperature approach instead.

Every resulting heating feed request is bounded by the physical maximum reported by the boiler. No software command can raise that limit. Before the maximum has been observed on CAN, the calculated request remains at the 10 °C off value.

### Calculate yourself

You can calculate a requested `FeedSetpoint` externally and enable `OverrideSetpoint`. The effective request is still bounded by the manufacturer-defined 10 °C off value and the physical maximum reported by the boiler; override does not bypass those limits.

[See Example JSON](#example-parameter-json-for-setting-heating-parameters) and look out for:
```json
  [...]
  "FeedSetpoint": 70,
  [...]
  "OverrideSetpoint": true
```

### Valve-based control

`ValveScalingOpening` is the supplied demand, normalized by `ValveScalingMaxOpening` and bounded to 0–100%. The firmware does not aggregate individual room valves: the producer must preserve sustained genuine room demand rather than assume that a weighted average or an unfiltered maximum always represents it.

With `DynamicAdaption` disabled, static interpolation remains between `FeedMinimum + Adaption` and the boiler's reported `FeedMaximum`, followed by the output bounds.

With both valve scaling and dynamic adaptation enabled, the outside curve supplies the interpolation baseline:

```text
r = clamp((FeedCutOff - outside) / (FeedCutOff - FeedBaseSetpoint), 0, 1)
rw = min(r, WeatherSupportMaximum / 100)
D = clamp(ValveScalingOpening / ValveScalingMaxOpening, 0, 1)
weatherBase = clamp(FeedMinimum + (FeedMaximum - FeedMinimum) * rw
                    + Adaption * rw, 10, FeedMaximum)
feed = weatherBase + (FeedMaximum - weatherBase) * D
```

This supplies an outside-dependent baseline even with zero manual adaptation and allows valve demand to raise it toward the boiler maximum. `WeatherSupportMaximum` limits the weather fraction; it does not multiply the entire curve or change the physical boiler limit. The default is 60%, 100% restores the full hybrid curve, and 0% removes its weather-weighted baseline/adaptation contribution. The setting affects dynamic valve control only; static valve control, weather-only control, override, boost and fail-safe hold retain their own behavior. Missing or malformed stored settings use 60%; control commands reject invalid settings.

At the default 60%, weather support becomes flat below approximately +2.4 °C with the −10/+21 °C curve endpoints. Valve demand remains effective below that point. This deliberately exchanges further weather-driven increases for demand-driven increases; 60% is an initial setting, not a verified winter calibration. No heating enablement or zero valve demand yields the 10 °C off request, including during boost/override in valve mode. Missing/invalid required weather inputs or an invalid curve produce off rather than an invented heating temperature.

Calculations retain floating-point precision until nearest-half-degree rounding. The representable output ceiling is rounded down where necessary so it cannot exceed the boiler maximum. For minimum 10 °C, maximum 75 °C, basepoint −10 °C, cutoff 21 °C, adaptation zero and demand 32%:

| Outside temperature | Effective hybrid feed request |
| --- | ---: |
| 11.59 °C | 44.0 °C |
| 11.5 °C | 44.5 °C |
| 0 °C | 57.5 °C |
| −10 °C | 57.5 °C |

These are calculation examples, not a calibration guarantee for every building. The configured `Features.UseAuxiliaryOutsideTempReference` determines whether the outside input comes from CAN or the externally supplied temperature.

```json
{
  "ValveScaling": true,
  "DynamicAdaption": true,
  "ValveScalingMaxOpening": 100,
  "WeatherSupportMaximum": 60,
  "ValveScalingOpening": 32,
  "Adaption": 0
}
```

#### Heating off and external schedules

In valve mode, `Enabled: false` or zero supplied valve demand lowers the effective request to 10 °C immediately, before override, boost or forecast assistance. The separate [low-setpoint off delay](assets/Configuration.md#low-setpoint-off-delay) latches the controller's heating operation off after a sustained low request; it is not a delay before lowering the request or a boiler pump-afterrun timer.

External scheduling remains the producer's responsibility during normal MQTT-connected operation. In the documented Home Assistant installation, the accepted policy is **not an absolute 23:30 stop**: a season lock takes priority, otherwise the 05:30–23:30 window **or** continuing valve demand permits the desired-operation helper to stay on. That helper is not the device's `Enabled` state. Demand hysteresis can stop the device before every physical valve closes, and room week plans do not switch all valves instantaneously.

Read device enablement, effective feed request, burner flame and heating-pump state separately; a reported `HeatingOperation` value alone does not prove that the burner or pump is running. See the [installed-system policy and observed shutdown example](assets/Examples/HomeAssistant/README.md#installed-system-scheduling-and-shutdown) for the helper/device distinction. The firmware's [offline fail-safe schedule](assets/Configuration.md#offline-fail-safe) is a separate fallback, not an additional online hard stop.

### Optional three-hour weather forecast

`ForecastEnabled` defaults to `false`. `ForecastCoefficient` defaults to `0.5` and accepts 0–1 °C feed per °C predicted outside-temperature change. These are separate from the maximum weather fraction. Stable MQTT/Home Assistant changes persist; web runtime controls remain temporary.

The producer averages the three upcoming hourly temperatures and subtracts the **same provider's current temperature**, avoiding a permanent bias between the weather station and the boiler's outside sensor:

```text
temperatureChange = mean(next three hourly provider temperatures) - providerCurrent
correction = clamp(-ForecastCoefficient * temperatureChange, -2, 2)
```

Cooling supplies positive support; warming supplies negative correction. Static and dynamic valve modes permit reductions only when the same fresh sample explicitly confirms `ForecastRoomsSatisfied: true` for **all relevant rooms**. A weighted valve average cannot provide that permission. Unknown room states, missing measurements, deficits or insufficient freshness must deny it. Weather-only control retains both correction signs. Disabled heating, zero valve demand, override, boost and fail-safe hold ignore forecast assistance; the observed physical boiler maximum still bounds every output.

Samples arrive through [`POST /api/forecast`](assets/API.md#get-apiforecast-and-post-apiforecast) or MQTT, expire locally from receipt time, and are never restored after reboot. Already-computed `Correction` remains supported without applying the coefficient twice. Available forecast telemetry is not proof that a heating mode applies it.

The optional [Home Assistant producer](assets/Examples/HomeAssistant/README.md) retrieves hourly forecasts outside the ESP32 control loop, renews non-retained samples and revokes reduction permission on room changes. It is installed disabled; enable it only after deploying compatible firmware and checking its weather/room/topic selection.

Forecast samples cannot directly enable heating or bypass the off/zero-demand guards. Their effect on feed temperature can nevertheless influence subsequent room temperatures and valve demand; this does not establish that they have no indirect effect on shutdown timing.

### Temperature tests and simulation

The real temperature calculation is shared with isolated Unity tests and a serial JSON simulator. They run on the test ESP32 without CAN, WiFi, MQTT or heater I/O. See [`test/README`](test/README) for the COM3 commands and custom input/output scenarios.

Builds use the pinned official `platformio/espressif32@7.1.3` platform (Arduino 2.0.17) with C++17. Keep PlatformIO and its uploader dependencies in the same Python environment; use `python -m platformio` when several Python/PlatformIO installations coexist.

## Updating

The standard "Arduino OTA" procedure is included which means you can upload the code to your ESP32 without having to plug in the USB cable. See `platformio.ini` and modify the IP address accordingly.

You can also use the web UI (See: Firmware Update on the menu bar) to upload the `firmware.bin` and `littlefs.bin` files to update the firmware and filesystem image.

- A firmware-only update preserves the existing LittleFS configuration.
- Every successfully loaded or saved device configuration is mirrored to the separate NVS partition. After a WebUI, PlatformIO, esptool, or programmer-based LittleFS replacement, startup recognizes the credential-free provisioning template and restores the last valid device configuration before normal loading. WebUI filesystem updates still refresh and validate the backup before writing the image.
- A deliberately uploaded valid `/configuration.json` remains authoritative and replaces the persistent mirror after it loads. Keep a downloaded backup for manual disaster recovery. To intentionally start over, hold BOOT for 10 seconds during startup; erasing the NVS partition or the whole flash also produces a genuine factory reset.
- Configuration saves from the web UI are written atomically and verified before they replace the previous file. If validation or writing fails, the previous configuration remains available as a backup.

## Telnet Console

Debug info can be retrieved using a very basic telnet implementation. Simply connect to the ESP32 using telnet and watch as the messages flow. You can reboot the ESP by typing `reboot` and press enter. Be aware you have to type very quickly because this is truly a very minimalistic and barebone implementation of a client-server console communication which is primarily designed to see debug output without having to stand near the esp.

## Home Assistant Integration

Cerasmarter uses current MQTT **device discovery**. No Home Assistant YAML files or separate discovery templates are required.

Set `HomeAssistant.Enabled` to `true`, configure the same discovery prefix used by Home Assistant (normally `homeassistant`), and give every controller a unique `DeviceId`. After connecting to MQTT, the controller publishes one retained device-discovery document to:

```text
<AutoDiscoveryPrefix>/device/<DeviceId>/config
```

Home Assistant groups the heating, hot-water, status, and auxiliary-temperature entities under one device. The integration provides temperature and status sensors, binary operating-state sensors, dynamically generated auxiliary-sensor entities, and a read-only sensor for the remaining boost time reported by the controller logic.

Heating controls cover the complete runtime command contract used by the default `cerasmarter/heating/parameters` topic: heating enablement, requested feed setpoint and direct-setpoint override, heating-curve basepoint/cutoff/minimum, manual and dynamic adaptation, room-reference/target/auxiliary temperatures, valve-scaling enablement/current/max opening, boost duration, boost, and fast heatup. Home Assistant uses individual command topics below `cerasmarter/<DeviceId>/Heating/`, while legacy clients can continue publishing the existing combined JSON document. Both paths use the same validation and immediately publish the accepted state back to Home Assistant.

Memory, filesystem and flash capacity, chip model and revision, CPU core count, CPU frequency, and auxiliary-sensor connectivity are exposed as diagnostic entities. Home Assistant keeps these on the device's diagnostic card instead of treating them as normal heating controls or measurements.

Discovery assigns purpose-specific Material Design icons to every entity. The **Flame Lit** binary sensor intentionally has no Home Assistant device class: it therefore reports plain **On/Off** while using a flame icon whose active color follows the burner state. MQTT discovery supports one static icon per entity, so using different custom glyphs for its on and off states would require a separate Home Assistant template entity.

Runtime state and command topics use `cerasmarter/<DeviceId>/...`, independently of the discovery prefix. Discovery and state messages are retained, and MQTT Last Will availability marks the device offline if its connection is lost. The controller also listens for Home Assistant's `<AutoDiscoveryPrefix>/status` birth message and republishes discovery when Home Assistant restarts.

![Home Assistant device created through MQTT discovery](/assets/home-assistant-autodiscovery-device.png)

Keep `DeviceId` stable after discovery. If it or the discovery prefix is changed through the web interface, the controller removes its previous retained discovery record and reconnects with the new identity. An old record only needs manual removal if the broker was unavailable during the change.

See the [configuration guide](assets/Configuration.md#home-assistant) for all settings and the [API reference](assets/API.md#mqtt-api) for topic behavior.

## Hints
- Automatic takeover is suppressed while messages are observed in the configurable controller-address range. For capture-only installations, enable `CAN.ReadOnly` in the web interface or configuration file; this hard interlock cannot time out and re-enable transmission.
- For debug purposes the `configuration.General.Debug` variable controls whether you see verbose output from routines such as feed-temperature calculation and step-chain progress.
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

For local USB provisioning only, set `CERASMARTER_CONFIG_FILE` to an external configuration file while running the `buildfs`/`uploadfs` targets. This stages that file in the generated image without adding it to the repository. The pipeline stamps it with `"ProvisioningTemplate": false`, making the bundled configuration authoritative over an older NVS mirror on first boot; it then becomes the new persistent backup. The GitHub release workflow never sets this override and verifies that release images use the credential-free template stamped `true`.

This distinction also covers unusual provisioning edge cases: a custom filesystem configuration may intentionally have empty Wi-Fi credentials, but the explicit `false` marker still prevents it from being replaced. If constructing a LittleFS image outside the supplied pipeline, set the root-level marker yourself. Be aware that custom images contain the selected configuration in recoverable form—including Wi-Fi or MQTT secrets—and must not be published as release artifacts.

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
- The maintainers of the ESP32Async ESPAsyncWebServer and AsyncTCP libraries: https://github.com/ESP32Async/ESPAsyncWebServer
- The Chart.js contributors. Chart.js 4.5.1 is bundled locally under the MIT license for dashboard history charts: https://www.chartjs.org/
