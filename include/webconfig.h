#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <wifi_config.h>
#include <configuration.h>

extern AsyncWebServer *server;

extern void StartApMode();

extern void notFound(AsyncWebServerRequest *request);

extern void ConfigureAndStartWebserver();

extern String processor(const String &var);

extern String humanReadableSize(const size_t bytes);

extern String listFiles(bool ishtml);

extern volatile bool ShouldReboot;

extern void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool);


#endif