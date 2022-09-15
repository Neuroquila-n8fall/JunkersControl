#ifndef _WIFI_CONFIG_H
#define _WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>

//——————————————————————————————————————————————————————————————————————————————
//  WiFi Settings
//——————————————————————————————————————————————————————————————————————————————

//-- WiFi Check interval for status output
extern const int wifiRetryInterval;
//-- Wifi Client object
extern WiFiClient espClient;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void connectWifi();


#endif