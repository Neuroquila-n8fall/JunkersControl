#ifndef _CAN_PROCESSOR_H
#define _CAN_PROCESSOR_H

#include <Arduino.h>
#include <ACAN2515.h>
//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 SPI Pin Assignment
//——————————————————————————————————————————————————————————————————————————————

static const byte MCP2515_SCK = 18;  // SCK input of MCP2515
static const byte MCP2515_MOSI = 23; // SDI input of MCP2515
static const byte MCP2515_MISO = 19; // SDO output of MCP2515
static const byte MCP2515_CS = 5;    // CS input of MCP2515 (adapt to your design)
static const byte MCP2515_INT = 17;  // INT output of MCP2515 (adapt to your design)

//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 Driver object
//——————————————————————————————————————————————————————————————————————————————

extern ACAN2515 can;

//——————————————————————————————————————————————————————————————————————————————
//  MCP2515 Quartz: adapt to your design
//——————————————————————————————————————————————————————————————————————————————

static const uint32_t QUARTZ_FREQUENCY = 16UL * 1000UL * 1000UL; // 16 MHz

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void setupCan();
extern void processCan();


#endif