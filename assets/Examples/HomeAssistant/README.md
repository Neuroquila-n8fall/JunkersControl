# Optional three-hour weather forecast

Home Assistant can provide forecast samples; the firmware does not fetch weather itself. Configure a producer through Home Assistant's UI or configuration API using your own weather provider, room entities and controller identity. Installation-specific YAML/JSON automation files are intentionally not shipped to protect privacy. Create the producer disabled and enable it only after checking its inputs and the firmware settings; do not add invalid top-level enablement fields or an `initial_state` override that disables it after every restart.

## Control settings

| Setting | Default | Meaning |
| --- | --- | --- |
| `WeatherSupportMaximum` | 60% | Maximum outdoor fraction in dynamic valve mode; separate from forecasts |
| `ForecastEnabled` | false | Explicit opt-in to applying fresh forecast samples |
| `ForecastCoefficient` | 0.5 | 0–1 °C feed per °C expected outdoor-temperature change |

Stable MQTT/Home Assistant settings persist. The web form uses temporary `/api/control/queued` commands. The physical maximum received from the boiler remains the final requested-temperature ceiling; this is not a claim that measured water temperature cannot overshoot its requested setpoint.

## Source and calculation

Choose a weather entity that provides hourly forecasts and a current temperature in Celsius. A producer can run every ten minutes and on weather/room/forecast-setting changes, call `weather.get_forecasts` with `type: hourly`, and select exactly three distinct timestamps in `(now, now + 3 hours]`. Invalid/missing temperatures, duplicate timestamps, insufficient hours, non-Celsius units or an excessively old provider report must produce a neutral, short-lived correction rather than invented forecast data. Keep location-bearing provider entity IDs in your private Home Assistant configuration, not in shared examples.

```text
change = mean(three future hourly temperatures) - current temperature of SAME provider
available correction = clamp(-ForecastCoefficient * change, -2, 2)
```

The provider's current temperature is deliberately not the boiler's measured outside temperature: their location/sensor offset must not become a permanent correction. The ESP32 keeps its measured outside input and derives the bounded correction locally. Already-computed `ForecastCorrection` is also accepted and is not scaled twice.

Cooling supports heating; warming can reduce it. In static/dynamic valve mode, reductions additionally require `ForecastRoomsSatisfied: true` on the same fresh sample. Disabled heating, zero valve demand, override, boost and fail-safe hold do not use the forecast.

## Room permission

Select all relevant room climate entities for your installation, not a weighted aggregate. Every selected entity must exist and be available. For rooms not explicitly off:

- `hvac_action` must explicitly report `idle` or `off`; `heating`, other active actions and missing/unknown actions deny permission, even if temperature already meets target.
- Current and target temperatures must be numeric.
- Current temperature must be at least the target.
- HA's last report must be no older than 15 minutes and not future-dated.

Missing/unknown data, any deficit, stale reports or an empty selection deny reduction permission. Event-driven thermostats may legitimately report unchanged temperatures infrequently; denying permission is conservative and **does not diagnose a frozen sensor**. Report age is a HA-data freshness policy, not proof of radio-sensor freshness.

Room/forecast-enable transitions first publish a neutral sample to revoke any previous reduction permission, before provider retrieval can fail or wait. The ordinary periodic update does not repeatedly clear a valid correction. Weather retrieval failure leaves the previous sample only until its existing local expiry; invalid returned data explicitly clears it.

## Transport and expiry

Publish to your controller's configured combined heating-parameter topic. The documentation's default topic is `cerasmarter/heating/parameters`; confirm the actual value before use. Send samples with `retain: false`, for example:

```json
{
  "ForecastTemperatureChange": -4,
  "ForecastRoomsSatisfied": false,
  "ForecastValidForSeconds": 900
}
```

This contains no `Enabled`, valve opening, boiler maximum or stable forecast setting. A four-degree cooling trend at coefficient 0.5 makes +2 °C available. A +4 °C warming trend makes −2 °C available, but valve mode applies it only with room permission. Samples expire after 15 minutes from receipt, clear on reboot, and never persist. Do not retain them in MQTT: replay would start a new lifetime. See the [API reference](../../API.md#get-apiforecast-and-post-apiforecast).

## Activate intentionally

1. Deploy firmware containing the forecast settings and retain the controller's normal CAN safety profile.
2. Find the controller's forecast-enable switch and forecast-coefficient number in Home Assistant's entity registry. Actual IDs depend on device identity and may include area prefixes; do not copy installation-specific IDs or replace missing firmware entities with fake helpers.
3. Configure your producer with the selected weather provider, complete room selection, controller identity and actual MQTT topic through Home Assistant's UI or configuration API. Each producer must target the intended controller explicitly.
4. Start with coefficient 0.5, enable the firmware forecast switch, and explicitly enable your forecast producer. Both opt-ins are required.
5. Inspect the HA trace and `/api/forecast`: available correction, raw change, remaining lifetime and room permission. Compare calculated/requested feed with real sensor response; available correction does not prove it was applied.

Disable the firmware forecast switch to stop application immediately; disable the automation to stop publication. Turning off only the producer leaves an existing sample valid until its local expiry. Settings and automation enablement follow normal persistence after explicit activation.

## Scheduling and shutdown boundaries

The forecast producer handles **only forecast publication**. Scheduling, valve-demand and device-handoff automations are separate and must be configured for your own installation. A window-or-demand policy can be expressed as:

```text
desiredOperation = !seasonLock && (insideConfiguredWindow || valveDemand)
```

The season lock takes priority. Closing the configured time window does not impose an absolute stop if demand remains. Desired operation, hysteretic valve demand and the controller's actual `Enabled` value are separate signals; a device-handoff automation should confirm or recheck the device state rather than assuming that a helper proves enablement.

Demand thresholds and any weighting belong to the installation's private configuration. Crossing an off threshold can clear demand while some physical valves remain open. Room week plans and normal thermostat hysteresis do not require simultaneous global valve closure, and heating at or slightly above a room target is not by itself evidence of a fault.

Explicit disablement requests 10 °C immediately. The separate low-setpoint timer latches commanded operation off after a sustained low request; its general default is 300 seconds. Boiler pump afterrun is not a guaranteed fixed interval. See [configuration semantics](../../Configuration.md#low-setpoint-off-delay).

For fault interpretation, distinguish desired-operation/demand helpers, device `Enabled`, effective feed request, received `HeatingOperation`, burner flame and pump state. These are not interchangeable proofs of physical operation. Burner cycling alone does not establish gas consumption, and HA readback does not constitute an external CAN-wire acknowledgement.

The forecast producer publishes neither `Enabled` nor valve demand. Firmware off/zero-demand guards run before forecast assistance, so a sample cannot directly override shutdown. Forecast-driven feed changes can indirectly alter room and valve trajectories; observation alone does not prove zero indirect influence. The [offline fail-safe clock](../../Configuration.md#offline-fail-safe) is a different fallback used during sustained MQTT disconnection, not a second normal-operation stop clock.

## Verification boundaries

Native HA template evaluation covers cooling, warming, missing hours, wrong units, stale provider reports, satisfied/deficient/unavailable/stale rooms, missing targets and empty selection. The real ESP32 temperature and forecast-lifecycle suites cover gain, both signs, mode guards, permission changes, disablement, expiry and cross-queue ordering. These are not proof of winter comfort or weather-provider calibration. See [`test/README`](../../../test/README).
