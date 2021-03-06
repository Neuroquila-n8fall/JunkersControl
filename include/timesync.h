#ifndef _TIMESYNC_H
#define _TIMESYNC_H

#include <ezTime.h>

//——————————————————————————————————————————————————————————————————————————————
//  NTP Time Object
//——————————————————————————————————————————————————————————————————————————————
extern Timezone myTZ;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void SyncTimeIfRequired();
extern bool TimeIsSynced();


#endif