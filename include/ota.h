#ifndef _OTA_H
#define _OTA_H

#include <Arduino.h>
#include <ota.h>
#include <ArduinoOTA.h>
#include <telnet.h>
#include <can_processor.h>
#include <mqtt.h>
#include <configuration.h>

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