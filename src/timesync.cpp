#include <Arduino.h>
#include "timesync.h"
#include <ezTime.h>

//——————————————————————————————————————————————————————————————————————————————
//  NTP Time Object
//——————————————————————————————————————————————————————————————————————————————
Timezone myTZ;

bool AlarmIsSet = false;

//Sync using NTP, if clock is off
void SyncTimeIfRequired()
{
  // ezTime progresses through events() in the cooperative main loop. Never
  // call waitForSync() here: without internet its default timeout is infinite
  // and would stop CAN processing.
}

//Returns TRUE if the clock is on point and false if it requires calibration
bool TimeIsSynced()
{
  return timeStatus() == timeSet;
}
