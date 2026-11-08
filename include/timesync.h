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
// Local time as minutes since Monday 00:00; false until the clock has been set.
extern bool LocalMinuteOfWeek(int &minuteOfWeek);
extern void printWifiStatus();


#endif