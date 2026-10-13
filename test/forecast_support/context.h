#ifndef FORECAST_TEST_CONTEXT_H
#define FORECAST_TEST_CONTEXT_H
#ifdef __cplusplus
// Preinclude only for forecast_test: retain the real forecast implementation,
// replacing its application's CAN/network context with passive test bindings.
#define _MAIN_H
#define _MQTT_H
#define _HEATING_H
#define _CAN_PROCESSOR_H
#include "main.h"
#include "mqtt.h"
#include "heating.h"
#endif
#endif
