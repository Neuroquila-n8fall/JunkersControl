#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>
#include <ACAN2515.h>

extern void SendMessage(CANMessage msg);
extern void SetDateTime();
extern void Reboot();
extern CANMessage PrepareMessage(uint32_t id);

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
//  Pins
//——————————————————————————————————————————————————————————————————————————————

//Status LED Pin
extern const int Status_LED;

//Wifi Status LED Pin
extern const int Wifi_LED;

//MQTT Status LED Pin
extern const int Mqtt_LED;

extern const int Heating_LED;

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

//-- Date & Time Interval: 0...MAXINT, Ex.: '5' for a 5 second delay between setting time.
extern int dateTimeSendDelay;

//-- LED Helper Variables
extern bool statusLed;
extern bool wifiLed;
extern bool mqttLed;

//-- Timestamp of last received message from the heating controller
extern unsigned long lastHeatingMessageTime;

//-- Timestamp of the last message sent by us
extern unsigned long lastSentMessageTime;

//——————————————————————————————————————————————————————————————————————————————
//  Macros / Extensions
//——————————————————————————————————————————————————————————————————————————————

//-- runEvery Macro to get rid of timer/counter variables
//-- -- Source: https://forum.arduino.cc/t/runevery-the-next-blink-without-delay/122405
#define runEveryMilliseconds(t) for (static uint32_t _lasttime;\
    (uint32_t)((uint32_t)millis() - _lasttime) >= (t);\
    _lasttime += (t))

#define runEverySeconds(t) for (static uint32_t _lasttime;\
    (uint32_t)((uint32_t)millis() - _lasttime) >= (t) * 1000;\
    _lasttime += (t) * 1000)



#endif