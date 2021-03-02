#ifndef _TELNET_H
#define _TELNET_H

#include <Arduino.h>
#include <WiFi.h>

//——————————————————————————————————————————————————————————————————————————————
//  Server for remote console. We're using the telnet port here
//——————————————————————————————————————————————————————————————————————————————

const uint TelnetServerPort = 23;
WiFiServer TelnetServer(TelnetServerPort);
WiFiClient TelnetRemoteClient;

#endif