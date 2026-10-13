#include <Arduino.h>
#include <forecast.h>
#include <feed_temperature.h>
#include <main.h>
#include <can_processor.h>
#include <heating.h>
#include <mqtt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace
{
struct ForecastSample
{
  ForecastInput input;
  uint32_t receivedAt = 0;
  uint32_t lifetimeMillis = 0;
};
portMUX_TYPE forecastMux = portMUX_INITIALIZER_UNLOCKED;
ForecastSample pending;
ForecastSample active;
bool pendingAvailable = false;
uint32_t nextSubmission = 0;
uint32_t latestSubmission = 0;
bool enabled = false;
double coefficient = 0.5;
bool settingsChanged = false;
double lastEffectiveCorrection = 0.0;
bool lastRoomsSatisfied = false;
bool lastFresh = false;

ForecastState stateLocked(uint32_t now)
{
  const uint32_t elapsed = uint32_t(now - active.receivedAt);
  const bool fresh = active.lifetimeMillis != 0 && elapsed < active.lifetimeMillis;
  const double correction = fresh ? (active.input.TemperatureChange
      ? DeriveForecastCorrection(active.input.Value, coefficient) : active.input.Value) : 0.0;
  return {enabled, coefficient, correction,
          fresh ? (active.lifetimeMillis - elapsed + 999UL) / 1000UL : 0,
          fresh && active.input.RoomsSatisfied, fresh && active.input.TemperatureChange,
          fresh && active.input.TemperatureChange ? active.input.Value : 0.0};
}
}

uint32_t ReserveForecastSubmission()
{
  portENTER_CRITICAL(&forecastMux);
  if (++nextSubmission == 0)
    ++nextSubmission;
  const uint32_t submission = nextSubmission;
  portEXIT_CRITICAL(&forecastMux);
  return submission;
}

bool SubmitForecast(const ForecastInput &input)
{
  return SubmitForecast(input, millis());
}

bool SubmitForecast(const ForecastInput &input, uint32_t receivedAt, uint32_t submission)
{
  const double limit = input.TemperatureChange ? 50.0 : 2.0;
  if (!isfinite(input.Value) || input.Value < -limit || input.Value > limit ||
      input.ValidForSeconds < 1 || input.ValidForSeconds > 14400)
    return false;
  portENTER_CRITICAL(&forecastMux);
  if (!submission)
  {
    if (++nextSubmission == 0)
      ++nextSubmission;
    submission = nextSubmission;
  }
  // Outstanding submissions cannot span half the sequence space. Preserve
  // receipt order even when an older runtime command drains after newer input.
  if (latestSubmission && (submission == latestSubmission ||
      uint32_t(submission - latestSubmission) > UINT32_MAX / 2))
  {
    portEXIT_CRITICAL(&forecastMux);
    return false;
  }
  latestSubmission = submission;
  pending = {input, receivedAt, input.ValidForSeconds * 1000UL};
  pendingAvailable = true;
  portEXIT_CRITICAL(&forecastMux);
  return true;
}

bool SubmitForecastCorrection(double correction, uint32_t validForSeconds)
{
  return SubmitForecast({correction, validForSeconds, false, false});
}

void SetForecastSettings(bool newEnabled, double newCoefficient)
{
  if (!isfinite(newCoefficient) || newCoefficient < 0.0 || newCoefficient > 1.0)
    return;
  portENTER_CRITICAL(&forecastMux);
  settingsChanged |= enabled != newEnabled || coefficient != newCoefficient;
  enabled = newEnabled;
  coefficient = newCoefficient;
  portEXIT_CRITICAL(&forecastMux);
}

double GetForecastCorrection(uint32_t nowMillis)
{
  portENTER_CRITICAL(&forecastMux);
  const ForecastState state = stateLocked(nowMillis);
  portEXIT_CRITICAL(&forecastMux);
  return state.Enabled ? state.Correction : 0.0;
}

ForecastState GetForecastState()
{
  portENTER_CRITICAL(&forecastMux);
  const ForecastState state = stateLocked(millis());
  portEXIT_CRITICAL(&forecastMux);
  return state;
}

void ProcessForecastCorrection()
{
  portENTER_CRITICAL(&forecastMux);
  const bool acceptedSample = pendingAvailable;
  if (pendingAvailable)
  {
    active = pending;
    pendingAvailable = false;
  }
  const uint32_t now = millis();
  if (active.lifetimeMillis && uint32_t(now - active.receivedAt) >= active.lifetimeMillis)
    active = ForecastSample{};
  const ForecastState state = stateLocked(now);
  const bool changedSettings = settingsChanged;
  settingsChanged = false;
  portEXIT_CRITICAL(&forecastMux);

  const double correction = state.Enabled ? state.Correction : 0.0;
  const bool roomsSatisfied = state.Enabled && state.RoomsSatisfied;
  const bool fresh = state.RemainingSeconds != 0;
  const bool recalculate = correction != lastEffectiveCorrection || roomsSatisfied != lastRoomsSatisfied || changedSettings;
  if (!acceptedSample && !recalculate && fresh == lastFresh)
    return;
  lastEffectiveCorrection = correction;
  lastRoomsSatisfied = roomsSatisfied;
  lastFresh = fresh;
  if (recalculate)
  {
    if (!SetupMode)
      SetFeedTemperature();
    else
      commandedValues.Heating.CalculatedFeedSetpoint = CalculateFeedTemperature();
  }
  PublishHeatingTemperaturesAndStatus();
}
