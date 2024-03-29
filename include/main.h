#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>
#include <ACAN2515.h>
// General Configuration
#include <configuration.h>
// Include MQTT Topics
#include <mqtt.h>
// CAN Module Settings
#include <can_processor.h>
// Telnet
#include <telnet.h>
// Heating Parameters
#include <heating.h>
// WiFi
#include <wifi_config.h>
// OTA
#include <ota.h>
// Temperature Sensors
#include <t_sensors.h>
// NTP Timesync
#include <timesync.h>
// Webconfig & Server
#include <webconfig.h>

#include <ESPmDNS.h>

// Mute MQTT Flag
#ifndef MUTE_MQTT
#define MUTE_MQTT 0
#endif

// Environment
#ifndef ENV
#define ENV "NOT_SET"
#endif

// Version
#ifndef VERSION
#define VERSION "LOCAL_BUILD"
#endif

//-- Preprocessor 
#define ST(A) #A
#define STR(A) ST(A)


extern void SendMessage(CANMessage msg);
extern void SetDateTime();
extern void Reboot();
extern CANMessage PrepareMessage(uint32_t id, int length = 8);
extern void WriteMessage(CANMessage msg, bool received = true);
extern bool SafeToSendMessage(bool dontWaitForController = true);

extern void ShowHeartbeat(void *pvParameter);
extern TaskHandle_t MqttActivityHandle;
extern void ShowMqttActivity(void *pvParameter);
extern TaskHandle_t CanErrorActivityHandle;
extern void ShowCanError(void *pvParameter);
extern void UpdateLeds(void *pvParameter);
extern void TrackBoostFunction(void *pvParameter);

//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

//This flag enables the control of the heating. It will be automatically reset to FALSE if another controller sends messages
//  It will be re-enabled if there are no messages from other controllers on the network for x seconds as defined by ControllerMessageTimeout
extern bool OverrideControl;

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

//-- WiFi Status Timer Variable
extern unsigned long wifiConnectMillis;

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

//-- CAN Error Counter
extern volatile int CanSendErrorCount;

//-- Setup Mode Flag
extern volatile bool SetupMode;

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