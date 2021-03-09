#ifndef _WIFI_CONFIG_H
#define _WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>

//——————————————————————————————————————————————————————————————————————————————
//  WiFi Settings
//——————————————————————————————————————————————————————————————————————————————
extern const char *ssid;
extern const char *pass;
extern const char hostName[];
//-- WiFi Check interval for status output
extern const int wifiRetryInterval;
//-- Wifi Client object
extern WiFiClient espClient;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void connectWifi();


#endif