#ifndef _LOGGER_H
#define _LOGGER_H

#include <Arduino.h>

class AsyncWebServer;

//——————————————————————————————————————————————————————————————————————————————
//  System log: Serial + telnet (live), RAM history (web page), and a small
//  reset-surviving event buffer. CAN traffic uses LogCan and stays live-only.
//——————————————————————————————————————————————————————————————————————————————

// Numeric values are a wire contract (MQTT "lvl", /api/logs level column).
enum LogLevel
{
    Error,
    Warn,
    Info,
    Debug,
    Verbose
};

extern void LogBegin();
extern void LogService();
extern void RegisterLogRoutes(AsyncWebServer *server);

extern void LogWrite(LogLevel level, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern void LogError(const char *format, ...) __attribute__((format(printf, 1, 2)));
extern void LogWarn(const char *format, ...) __attribute__((format(printf, 1, 2)));
extern void LogInfo(const char *format, ...) __attribute__((format(printf, 1, 2)));
// No-op unless configuration.General.Debug is set.
extern void LogDebug(const char *format, ...) __attribute__((format(printf, 1, 2)));
// CAN trace: Serial + telnet only, never stored.
extern void LogCan(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
