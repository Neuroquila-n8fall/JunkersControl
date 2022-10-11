#ifndef _TELNET_H
#define _TELNET_H

#include <Arduino.h>
#include <WiFi.h>
#include <Print.h>

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
extern void ReadFromTelnet();

/// @brief Enables Writing to Serial and Telnet at once.
class ConsoleWriter : public Print
{
public:
    size_t write(byte a)
    {
        if (TelnetRemoteClient.connected() && TelnetRemoteClient.available() == 0)
        {
            TelnetRemoteClient.write(a);
        }
        return Serial.write(a);
    }
};

/// @brief Used to Log messages to both Serial and Telnet
extern ConsoleWriter Log;

#endif