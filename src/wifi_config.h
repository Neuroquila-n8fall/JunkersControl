#ifndef _WIFI_CONFIG_H
#define _WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <arduino_secrets.h>

//——————————————————————————————————————————————————————————————————————————————
//  WiFi Settings
//——————————————————————————————————————————————————————————————————————————————
const char *ssid = SECRET_SSID;
const char *pass = SECRET_PASS;
const char hostName[] = "FXHEATCTRL01";
//-- WiFi Check interval for status output
const int wifiRetryInterval = 30000;
//-- Wifi Client object
WiFiClient espClient;

#endif