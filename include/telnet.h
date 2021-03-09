#ifndef _TELNET_H
#define _TELNET_H

#include <Arduino.h>
#include <WiFi.h>

//——————————————————————————————————————————————————————————————————————————————
//  Server for remote console. We're using the telnet port here
//——————————————————————————————————————————————————————————————————————————————

extern const uint TelnetServerPort;
extern WiFiServer TelnetServer;
extern WiFiClient TelnetRemoteClient;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void CheckForConnections();
extern void WriteToConsoles(String message);
extern void ReadFromTelnet();

#endif