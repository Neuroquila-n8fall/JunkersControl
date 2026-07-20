# Changelog

## v0.94.0

- Reworked the documentation around the current web UI, configuration persistence, MQTT/Home Assistant state, and CAN profiles. Added a consolidated HTTP/SSE/MQTT API reference with security and persistence notes, refreshed MQTT payload examples, and added current production-controller and Home Assistant discovery screenshots.
- Persist stable heating and hot-water settings received through MQTT or Home Assistant and restore them after reboot. Writes are coalesced to protect flash, while live measurements and momentary boost/fast-heat-up actions remain transient.

- Renamed the dashboard's derived burner-power percentage to "Feed temperature utilization" ("Vorlauftemperatur-Auslastung") to distinguish the feed-temperature ratio from the burner's actual thermal output.
- Fixed General Settings silently omitting disabled form fields and misreading typed JSON booleans, so every value now survives page reloads and is written to `configuration.json`. Added separately persisted normal-operation heating-curve basepoint and endpoint defaults, and audited the remaining configuration forms for the same disabled-field serialization error.
- Added conservative, non-blocking Wi-Fi roaming: connection attempts now scan all channels and select by RSSI, while sustained signals below -75 dBm trigger an asynchronous same-SSID scan and handover only when another AP is at least 12 dB better. A five-minute cooldown prevents access-point flip-flopping.
- Added functional CAN installation profiles for heating, mixed circuits, and domestic hot water; profiles gate decoding and control without assuming a particular room-controller model.
- Added a hard CAN read-only interlock, configurable controller-detection range, heartbeat address, and heartbeat interval.
- Added an explicit alternative domestic-hot-water address preset while retaining fully editable addresses and the established mapping. Credit for the underlying BM1 protocol observations: https://github.com/jen-s-and/JunkersControl.
- Rebuilt the CAN analyzer with always-available live events, timestamps, direction, filters, change-only view, per-ID cadence and payload statistics, candidate interpretations, bounded display, and JSON/CSV capture export. Added an explicit filter reset, corrected dark-mode table headers and fixed-navigation spacing, generalized the page description, and documented an effective and safe investigation workflow with production-controller screenshots.
- Modernized the dashboard into compact heating, mixed-circuit, domestic-hot-water, auxiliary, runtime-control, and system categories; disabled installation sections now disappear and the remaining cards reflow. Added derived burner load, heating-curve context beneath outside temperature, a dedicated auxiliary-sensor trend graph, a locally bundled MIT-licensed Chart.js 4.5.1 chart, corrected status-pill alignment, theme-aware chart labels, and explicit page scrolling.
- Fixed MQTT setpoint reporting so the effective heating value is no longer overwritten by the requested/default value, requested and effective Home Assistant values remain distinct, and unseen CAN setpoints are reported as unavailable instead of fabricated defaults. The transmitted domestic-hot-water setpoint now follows the accepted command rather than a hard-coded 10 °C payload. Addresses issue #39.
- Made automatic controller detection start read-only and wait through a complete configured timeout before taking control. The configurable, inclusive controller-address range recognizes the standard `0x250` operation frame and avoids startup and boundary-address override chatter. Addresses issue #40.
- Added a complete first-install release image containing the bootloader, project partition table, OTA metadata, firmware, and LittleFS. Updated installation guidance and CI artifacts so boards with an incompatible factory partition layout are flashed at `0x0` instead of receiving only unbootable component images. Addresses issue #48.
- Migrated older uploaded configurations by carrying forward newly introduced fail-safe, Home Assistant, timezone, and low-setpoint settings only when their keys are absent; explicit uploaded values remain authoritative. Upload responses now clarify that staged values become visible after configuration reload or reboot.
- Clamped every internally calculated feed setpoint to the manufacturer's exact 10 °C minimum before shutdown evaluation and CAN transmission.
- Made the validated NVS configuration backup persistent and refreshed it after every successful load or save, allowing raw LittleFS flashes to restore device settings while deliberate configuration uploads remain authoritative. Filesystem builds now stamp release templates and custom bundled configurations explicitly so edge-case provisioning intent is preserved. Added a 10-second BOOT-button factory-reset gesture that clears both NVS and LittleFS configuration copies.
- Fixed file-manager downloads to retain the original filename and extension instead of being saved as `file`.
- Added manufacturer-compatible low-setpoint shutdown: an effective 10 °C (50 °F) feed request disables heating after a configurable grace period, with a controller-side latch that cannot be defeated by repeated external enable commands.
- Changed offline fail-safe activation from a periodic-command lease to sustained MQTT disconnection. Home Assistant and other MQTT clients no longer need a Node-RED-style heartbeat, and reconnecting restores the previous runtime controls.
- Added the first live home dashboard for boiler, heating, hot-water, connectivity, and fail-safe state; the dashboard has now been expanded and its history chart replaced by the locally bundled Chart.js implementation described above.
- Restored percentage progress bars for RAM and application flash usage, and added a separate LittleFS usage indicator to the dashboard.
- Made the manual configuration reload action prominent in the file manager and clarified when it is still required.
- Added complete web fallback controls for every MQTT-controlled runtime value and consolidated both transports behind the same command handlers.
- Added `GET /api/runtime` and `POST /api/control` for current values and immediate heating control.
- Preserved valid device configuration automatically across LittleFS images uploaded through the web interface by using an NVS backup and boot-time restore.
- Added English and German web-interface localization with automatic browser-language defaults and persisted preferences.
- Reworked localization into one JSON resource per language with maintainable, area-based keys and explicit `data-i18n` attribute support, following the structure documented by the Javascript i18n core project.
- Added a persistent light/dark appearance switch to every web-interface page, with the operating-system preference used as the initial default.
- Replaced the dark-mode text control with a compact sun/moon icon and normalized MQTT form columns so translated labels wrap without displacing inputs.
- Completed the Cerasmarter branding migration by renaming Home Assistant runtime topics to `cerasmarter/<DeviceId>/...` and the local provisioning override to `CERASMARTER_CONFIG_FILE`.
- Renamed generated GitHub release artifacts to `Cerasmarter-<tag>`; historical repository URLs remain unchanged because the GitHub repository itself has not been renamed.
- Enhanced the CAN message analyzer to show the configured controller value name beside every known CAN address and captured message.
- Replaced the incomplete legacy Home Assistant integration with current MQTT device discovery using one retained device payload.
- Removed manual Home Assistant YAML and filesystem discovery-template files.
- Added discovery for heating, hot-water, controller-status, and dynamic auxiliary-temperature entities under one device.
- Added MQTT number controls for requested feed temperature, boost duration, and room-reference temperature.
- Added MQTT switches for heating enablement, boost, and fast heatup.
- Completed the Home Assistant heating command surface with heating-curve, manual and dynamic adaptation, target and auxiliary temperatures, direct-setpoint override, and valve-scaling controls.
- Added a Home Assistant sensor for the controller's remaining boost time and mirrored all accepted heating command values in the retained heating state payload.
- Centralized MQTT, Home Assistant, and web command validation, including safe temperature ranges and a nonzero maximum valve opening.
- Added retained online/offline availability through MQTT Last Will and automatic rediscovery after Home Assistant restarts.
- Added Home Assistant configuration to the web interface and automatic MQTT reconnection after saving it.
- Added cleanup of retained discovery records when Home Assistant discovery is disabled or its device identity changes.
- Added meaningful Home Assistant icons for every discovered entity and changed the burner flame entity to explicit on/off semantics with a flame icon.
- Changed the Home Assistant device manufacturer and discovery origin branding to Cerasmarter.
- Added Home Assistant diagnostic entities for heap memory, filesystem and flash storage, chip model and revision, CPU cores, CPU frequency, and auxiliary-sensor connectivity.
- Fixed MQTT command handling, including the previously unreachable hot-water parameter handler and unsafe callback payload termination.
- Prevented stale generated filesystem files from leaking into release images and made preprocessing failures stop the build.

## v0.93.4

- Pinned release and development builds to the tested pioarduino ESP32 platform, and upgraded all GitHub Actions workflows to Node.js 24-compatible action versions.
- Fixed Setup Mode incorrectly reporting that the filesystem was missing when only `configuration.json` was absent. Setup Mode now checks for the web frontend, allowing a fresh controller to start its access point and accept a configuration upload.
- Added the credential-free configuration template to generated LittleFS images so a freshly flashed controller enters AP provisioning immediately.
- Added distinct startup diagnostics for an unmountable filesystem, an empty filesystem, a missing web frontend, and a missing or invalid configuration.
- Made WiFi and MQTT reconnection cooperative and bounded, and removed the infinite NTP synchronization wait from the CAN control path.
- Fixed CAN receive queue draining, the outgoing-message interval calculation, and an uninitialized message sent during the feed-temperature step.
- Changed the heating LED to follow the decoded burner flame state while retaining CAN-error blinking.
- Replaced the non-functional hard-coded fallback with a configurable daily fail-safe, command lease, safe feed-temperature limits, offline timezone rules, and fresh-command recovery. Controllers boot in fail-safe mode until a recognized heating command arrives.

- Updated ArduinoJson, DallasTemperature, AsyncTCP, and ESPAsyncWebServer for compatibility with the current ESP32 Arduino framework.
- Improved WiFi startup and reconnect handling on the updated framework.
- Made configuration saves atomic and verified, with backup recovery when a write or reload fails.
- Fixed configuration persistence for WiFi, MQTT, MQTT topics, CAN, auxiliary sensors, and LED settings.
- Prevented a web reboot from overwriting a newly uploaded configuration with stale in-memory values.
- Added a **Reload Configuration** action to the file manager so an uploaded configuration can be activated without a power cycle.
- Fixed auxiliary-sensor loading in the web UI and retained compatibility with both nested and flat API responses.
- Improved filesystem lifecycle handling during reboot and Home Assistant discovery.
- Modernized the GitHub release workflow with maintained action versions, reproducible PlatformIO builds, configuration-exclusion checks, archived firmware and filesystem artifacts, and automatic draft-release creation for tags.

Release filesystem images contain only the credential-free configuration template. Local USB provisioning can explicitly select an external device configuration without placing credentials in the repository; the release workflow verifies that this override is not used for published images.

## v0.9.3
Emergency fix for web UI: I forgot to commit the working version of the frontend pages so the forms actually work.

## v0.9.2
Some fixes and improvements have been made.
For example the Reboot button didn't actually reboot the system.

Also the status page will now reflect the actual error state of the CAN-module instead of just counting erroneous messages.

A few code style errors have been fixed and some deprecated code snippets replaced.

## v0.9.1
This release contains a much needed enhancement to the user friendliness.
The integrated web interface allows you to:
- View overall system status
- Manage files (i.e. download your configuration and upload it again)
- Upload firmware and filesystem images so you don't have to spin up Platformio everytime there is a new release.
- Configure everything via forms instead of dealing with a json file
- Watch CAN messages on your bus with automatic value change highlighting. This will help you identify new message types without having to spin up a full blown CAN analyzer.

### Install Instructions
- Upload firmware
- Upload filesystem
- Open the Web UI @ http://cerasmarter/
- Configure everything to your needs


### REST Api

With the addition of a web UI a REST Api has been introduced. You can query via GET on the endpoints to receive the currently active config or POST to store the config.

The Endpoints are located at:
/api/config
- /general
```json
{
  "heatingvalues": true,
  "watervalues": false,
  "auxvalues": true,
  "tz": "Europe/Berlin",
  "busmsgtimeout": 30,
  "debug": true,
  "sniffing": true
}
```
- /wifi
```json
{
  "wifi_ssid": "ssid",
  "wifi_pw": "",
  "hostname": "CERASMARTER"
}
```
- /canbus
```json
{
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
    "Power": "0x251",
    "Mode": "0x258",
    "Economy": "0x253"
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
- /leds
```json
{
  "wifi-led": 26,
  "status-led": 27,
  "mqtt-led": 14,
  "heating-led": 25
}
```
- /mqtt
```json
{
  "mqtt-server": "1.2.3.4",
  "mqtt-port": 1883,
  "mqtt-user": "mqtt",
  "mqtt-password": "mqtt"
}
```
- /auxsensors
```json
[
  [
    {
      "Label": "Feed",
      "IsReturnValue": false,
      "Address": [
        "0x28",
        "0x76",
        "0x51",
        "0x91",
        "0x42",
        "0x20",
        "0x01",
        "0xE3"
      ]
    },
    {
      "Label": "Return",
      "IsReturnValue": true,
      "Address": [
        "0x28",
        "0x6F",
        "0x9C",
        "0xF6",
        "0x42",
        "0x20",
        "0x01",
        "0xF6"
      ]
    },
    {
      "Label": "Exhaust",
      "IsReturnValue": false,
      "Address": [
        "0x28",
        "0x6D",
        "0x98",
        "0xF5",
        "0x42",
        "0x20",
        "0x01",
        "0x0F"
      ]
    },
    {
      "Label": "Ambient",
      "IsReturnValue": false,
      "Address": [
        "0x28",
        "0xBF",
        "0x39",
        "0x10",
        "0x42",
        "0x20",
        "0x01",
        "0x93"
      ]
    }
  ]
]
```

## v0.9.0
Not only I noticed that it's about time to optimize the heating system. Thanks to the input of @rejoe2 we were able to simplify things so it can be adopted by a broader audience!

This release is all about making things easier. No longer is it necessary to modify code to get this working. You only need VS Code, the Platformio extension and you're (mostly) good to go!

Previously I considered this project a PoC (Proof of Concept) - a foundation for others to build their own system on.

Please let us know if anything should be broken or you got trouble making it work.

Instructions for this release:
- See Readme for prerequisites
- Build firmware and filesystem images
- Upload firmware to ESP32
- Modify the configuration so it fits your environment
- Upload filesystem image to ESP

## What's Changed
* Merge Feature and Optimization Overhaul by @Neuroquila-n8fall in https://github.com/Neuroquila-n8fall/JunkersControl/pull/7


**Full Changelog**: https://github.com/Neuroquila-n8fall/JunkersControl/commits/v0.9.0
