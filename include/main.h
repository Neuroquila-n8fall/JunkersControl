#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>



//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

//This flag enables the control of the heating. It will be automatically reset to FALSE if another controller sends messages
//  It will be re-enabled if there are no messages from other controllers on the network for x seconds as defined by ControllerMessageTimeout
extern bool Override;

//Controller Message Timeout
//  After this timeout this controller will take over control.
extern const int controllerMessageTimeout;

//Set this to true to view debug info
extern bool Debug;

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————



//-- WiFi Status Timer Variable
extern unsigned long wifiConnectMillis;

//-- One-Second Interval Timer Variable
extern unsigned long oneSecondTimer;

//-- Five-Second Interval Timer Variable
extern unsigned long fiveSecondTimer;

//-- Thirty-Second Interval Timer Variable
extern unsigned long thirtySecondTimer;

//-- Last Controller Message timer
extern unsigned long controllerMessageTimer;

//-- Step-Counter
extern int currentStep;

#endif