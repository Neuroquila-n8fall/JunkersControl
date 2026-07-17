# Changelog

## v0.93.4

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
