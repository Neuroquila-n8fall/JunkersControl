#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <wifi_config.h>
#include <configuration.h>

extern AsyncWebServer *server;

extern AsyncEventSource *eventSource;

extern void StartApMode();

extern void notFound(AsyncWebServerRequest *request);

extern void ConfigureAndStartWebserver();

extern String humanReadableSize(const size_t bytes);

extern void listFsFiles(AsyncWebServerRequest *request, String path  = "/");

extern void getFsUsagePercent(AsyncWebServerRequest *request);

extern void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);

extern void getSystemStatus(AsyncWebServerRequest *request);

extern void onWifiConfigReceive(AsyncWebServerRequest *request, JsonVariant &json);

extern void getWifiConfig(AsyncWebServerRequest *request);

extern void sendJson(JsonDocument &doc, AsyncWebServerRequest *request);

extern void getWifiNetworks(AsyncWebServerRequest *request);

extern void getCurrentWifiNetwork(AsyncWebServerRequest *request);

extern void getMqttConfig(AsyncWebServerRequest *request);

extern void onMqttConfigReceive(AsyncWebServerRequest *request, JsonVariant &json);

extern void getMqttTopicConfig(AsyncWebServerRequest *request);

extern void onMqttTopicConfigReceive(AsyncWebServerRequest *request, JsonVariant &json);

extern void getSystemInfo(AsyncWebServerRequest *request);

extern void getGeneralConfig(AsyncWebServerRequest *request);

extern void onGeneralConfigReceive(AsyncWebServerRequest *request, JsonVariant &json);

extern void configureGeneralEndpoints();

extern void configureGeneralApiEndpoints();

extern void configureFilemanagerEndpoints();

extern void configureFirmwareEndpoints();

extern void configureMqttEndpoints();

extern void configureWifiEndpoints();

extern volatile bool ShouldReboot;

extern void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool);

extern void configureCanConfigEndpoints();

extern void getCanbusConfig(AsyncWebServerRequest *request);

extern void onCanbusConfigReceive(AsyncWebServerRequest *request, JsonVariant &json);

extern void onAuxSensorsConfigReceive(AsyncWebServerRequest *request, JsonVariant &json);

extern void getAuxSensorsConfig(AsyncWebServerRequest *request);

extern void configureAuxSensorsEndpoints();

extern void onLedConfigReceive(AsyncWebServerRequest *request, JsonVariant &json);

extern void getLedConfig(AsyncWebServerRequest *request);

extern void configureLedConfigEndpoints();

#endif