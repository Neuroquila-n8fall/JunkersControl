#ifndef RUNTIME_CONTROL_TEST_CONTEXT_H
#define RUNTIME_CONTROL_TEST_CONTEXT_H

// PlatformIO force-includes this file in dependency translation units too.
// Only project translation units have the project's public headers available.
#if __has_include(<mqtt.h>)
// Keep production command/configuration/heating data types while replacing
// only main-loop and peripheral declarations.
#include <mqtt.h>
#define _CAN_PROCESSOR_H
#define _MAIN_H
#include <configuration.h>
#include <heating.h>
#include <failsafe.h>

extern volatile bool SetupMode;
extern bool OverrideControl;
extern TaskHandle_t MqttActivityHandle;
extern void ShowMqttActivity(void *parameter);
extern bool WriteConfiguration();
extern void SetFeedTemperature();
extern double CalculateFeedTemperature();
#endif

#endif
