#ifndef _TIMESYNC_H
#define _TIMESYNC_H

#include <ezTime.h>

//——————————————————————————————————————————————————————————————————————————————
//  NTP Time Object
//——————————————————————————————————————————————————————————————————————————————
extern Timezone myTZ;

extern bool AlarmIsSet;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void SyncTimeIfRequired();
extern bool TimeIsSynced();
extern void printWifiStatus();


#endif