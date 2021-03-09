#include <Arduino.h>
#include "timesync.h"
#include <ezTime.h>

//——————————————————————————————————————————————————————————————————————————————
//  NTP Time Object
//——————————————————————————————————————————————————————————————————————————————
Timezone myTZ;

//Sync using NTP, if clock is off
void SyncTimeIfRequired()
{
  //Sync Time if required
  timeStatus_t timeStat = timeStatus();
  if (timeStat != timeSet)
  {
    waitForSync();
  }
}

//Returns TRUE if the clock is on point and false if it requires calibration
bool TimeIsSynced()
{
  return timeStatus() == timeSet;
}

