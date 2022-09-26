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

// Mute MQTT Flag
#ifndef MUTE_MQTT
#define MUTE_MQTT 0
#endif

extern void SendMessage(CANMessage msg);
extern void SetEconomyMode(void *parameter);
extern void SetGuidance(void *parameter);
extern void SetFeedTemperatureTask(void *parameter);
extern void SetDhwNow(void *parameter);
extern void Reboot();
extern CANMessage PrepareMessage(uint32_t id, int length = 8);
extern void WriteMessage(CANMessage msg);
extern bool SafeToSendMessage(bool dontWaitForController = true);
extern void UpdateLeds(void *parameter);
extern void HeatingHousekeeping(void *parameter);
extern void PublishValues(void *parameter);
extern void SetDateTime(void *parameter);
extern void WriteMessage(CANMessage msg);
extern void KeepAlive(void *parameter);
extern void SetDhwSetpoint(void *parameter);
extern void ShowHeartbeat(void *pvParameter);
extern int _log_vprintf(const char *fmt, va_list args);

//——————————————————————————————————————————————————————————————————————————————
//  Task Handles
//——————————————————————————————————————————————————————————————————————————————

extern TaskHandle_t HeatingHousekeepingHandle;
extern TaskHandle_t UpdateLedsHandle;
extern TaskHandle_t ReadTemperaturesHandle;
extern TaskHandle_t PublishTemperaturesHandle;
extern TaskHandle_t PublishHeatingHandle;
extern TaskHandle_t PublishWaterHandle;
extern TaskHandle_t PublishStatusHandle;
extern TaskHandle_t RegularMqttHandle;
extern TaskHandle_t ReconnectMqttHandle;

extern TaskHandle_t SetEconomyModeHandle;
extern TaskHandle_t SetGuidanceHandle;
extern TaskHandle_t SetFeedTemperatureTaskHandle;
extern TaskHandle_t SetDhwNowHandle;
extern TaskHandle_t SetDateTimeHandle;
extern TaskHandle_t KeepAliveHandle;
extern TaskHandle_t SetDhwSetpointHandle;
extern TaskHandle_t SyncTimeTaskHandle;

//——————————————————————————————————————————————————————————————————————————————
//  Operation
//——————————————————————————————————————————————————————————————————————————————

//This flag enables the control of the heating. It will be automatically reset to FALSE if another controller sends messages
//  It will be re-enabled if there are no messages from other controllers on the network for x seconds as defined by ControllerMessageTimeout
extern bool Override;

//Controller Message Timeout
//  After this timeout this controller will take over control.
extern int controllerMessageTimeout;

//Set this to true to view debug info
extern bool Debug;

//——————————————————————————————————————————————————————————————————————————————
//  Variables
//——————————————————————————————————————————————————————————————————————————————

static bool writingToConsoles = false;

//-- WiFi Status Timer Variable
extern unsigned long wifiConnectMillis;

//-- Last Controller Message timer
extern unsigned long controllerMessageTimer;

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