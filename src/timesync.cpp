#include <Arduino.h>
#include "timesync.h"
#include <ezTime.h>
#include <WiFi.h>

//——————————————————————————————————————————————————————————————————————————————
//  NTP Time Object
//——————————————————————————————————————————————————————————————————————————————
Timezone myTZ;

bool AlarmIsSet = false;

//Sync using NTP, if clock is off
void SyncTimeIfRequired(void *pvParameter)
{
  while (true)
  {
    // Skip if we're not connected to a network.
    if(!WiFi.isConnected()) {
      vTaskDelay(1000/portTICK_PERIOD_MS);
      break;
    }

    // Sync Time if required
    timeStatus_t timeStat = timeStatus();
    if (timeStat != timeSet)
    {
      log_v("Syncing time.");
      waitForSync();
    }
    else {
      log_v("All good. Timesync not required.");
    }
    vTaskDelay(30000/portTICK_PERIOD_MS);
  }
}

//Returns TRUE if the clock is on point and false if it requires calibration
bool TimeIsSynced()
{
  return timeStatus() == timeSet;
}