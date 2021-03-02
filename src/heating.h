#ifndef _HEATING_H
#define _HEATING_H

#include <Arduino.h>
#include <main.h>

//——————————————————————————————————————————————————————————————————————————————
//  Constants for heating control
//——————————————————————————————————————————————————————————————————————————————

//Basepoint for linear temperature calculation
const int calcHeatingBasepoint = -15;

//Endpoint for linear temperature calculation
const int calcHeatingEndpoint = 21;

//Minimum Feed Temperature. This is the base value for calculations. Setting this as the setpoint will trigger the economy mode.
const int calcTriggerAntiFreeze = 10;

//-- Heating Scheduler. Fallback values for when the MQTT broker isn't available
HeatingScheduleEntry fallbackStartEntry = {5, 30, 0, true};
HeatingScheduleEntry fallbackEndEntry = {23, 30, 0, false};


#endif