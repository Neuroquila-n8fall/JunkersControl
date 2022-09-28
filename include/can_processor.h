#ifndef _CAN_PROCESSOR_H
#define _CAN_PROCESSOR_H

#include <Arduino.h>
#include <ACAN2515.h>

//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 Driver object
//——————————————————————————————————————————————————————————————————————————————

extern ACAN2515 can;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void setupCan();
extern void processCan();
extern void SetFeedTemperature();

#endif