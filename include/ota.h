#ifndef _OTA_H
#define _OTA_H

#include <ArduinoOTA.h>


//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

//-- OTA Flag
extern bool otaRunning;


//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

//OTA Handling
extern void ota();

#endif