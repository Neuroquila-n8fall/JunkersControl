#ifndef _TELNET_H
#define _TELNET_H

#include <Arduino.h>
#include <WiFi.h>

//——————————————————————————————————————————————————————————————————————————————
//  Server for remote console. We're using the telnet port here
//——————————————————————————————————————————————————————————————————————————————

extern const uint TelnetServerPort;
extern WiFiServer TelnetServer;

//——————————————————————————————————————————————————————————————————————————————
//  Functions
//——————————————————————————————————————————————————————————————————————————————

extern void CheckForConnections();
extern void ReadFromTelnet();

/// @brief Queues bytes for the telnet client. Callable from any task, never touches the socket.
/// Whole writes only: returns false (and counts a dropped line) when the queue is full or no client is connected.
extern bool TelnetWrite(const char *data, size_t length);
/// @brief Sends queued bytes without blocking. Main loop only.
extern void TelnetFlush();

#endif
