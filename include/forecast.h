#ifndef FORECAST_H
#define FORECAST_H

#include <stdint.h>

struct ForecastInput
{
  double Value = 0.0;
  uint32_t ValidForSeconds = 3600;
  bool TemperatureChange = false;
  bool RoomsSatisfied = false;
};
struct ForecastState
{
  bool Enabled;
  double Coefficient;
  double Correction; // Available correction; application also depends on heating mode.
  uint32_t RemainingSeconds;
  bool RoomsSatisfied;
  bool HasTemperatureChange;
  double TemperatureChange;
};
// Thread-safe submission; side effects remain in the main loop.
bool SubmitForecast(const ForecastInput &input);
bool SubmitForecastCorrection(double correction, uint32_t validForSeconds);
void SetForecastSettings(bool enabled, double coefficient);
void ProcessForecastCorrection();
// Reserve at receipt when another main-loop queue will forward the sample.
uint32_t ReserveForecastSubmission();
bool SubmitForecast(const ForecastInput &input, uint32_t receivedAtMillis, uint32_t submission = 0);
double GetForecastCorrection(uint32_t nowMillis);
ForecastState GetForecastState();

#endif
