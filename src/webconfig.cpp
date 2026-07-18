#include <webconfig.h>
#include <failsafe.h>

AsyncWebServer *server;
AsyncEventSource *eventSource;

volatile bool ShouldReboot = false;

struct UploadResult
{
    int status;
    const char *message;
};

static const char *configurationUploadFileName = "/configuration.upload";

static bool validateConfigurationFile(const char *path, String &errorMessage)
{
    File file = LittleFS.open(path, FILE_READ);
    if (!file)
    {
        errorMessage = "Configuration file could not be opened.";
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        errorMessage = "Configuration contains invalid JSON: " + String(error.c_str());
        return false;
    }

    const char *requiredSections[] = {
        "Wifi", "MQTT", "Features", "Time", "General", "HomeAssistant",
        "CAN", "AuxiliarySensors", "LEDs"};
    for (const char *section : requiredSections)
    {
        if (!doc[section].is<JsonObject>())
        {
            errorMessage = "Configuration section is missing or invalid: " + String(section);
            return false;
        }
    }

    return true;
}

static bool sendConfigurationSaveResult(AsyncWebServerRequest *request, const char *successMessage)
{
    if (WriteConfiguration())
    {
        request->send(200, "application/json", successMessage);
        return true;
    }

    request->send(500, "application/json", R"({"status":500,"msg":"Configuration could not be saved."})");
    return false;
}

void StartApMode()
{
    // Make sure we're disconnected
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(configuration.Wifi.Hostname, NULL);
    Serial.printf("\e[1;32mWiFi AP launched. Find me @ %s\r\n\e[0m", WiFi.softAPIP().toString().c_str());
}

void notFound(AsyncWebServerRequest *request)
{
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    request->send(404, "text/plain", "Not found");
}

void sendJson(JsonDocument &doc, AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

void ConfigureAndStartWebserver()
{
    server = new AsyncWebServer(80);
    eventSource = new AsyncEventSource("/events");

    server->addHandler(eventSource);

    server->onNotFound(notFound);

    configureFirmwareEndpoints();

    configureGeneralApiEndpoints();

    configureGeneralEndpoints();

    configureFilemanagerEndpoints();

    configureMqttEndpoints();

    configureWifiEndpoints();

    configureCanConfigEndpoints();

    configureAuxSensorsEndpoints();

    configureLedConfigEndpoints();

    server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
               {
                   request->send(LittleFS, "/frontend/reboot.html", "text/html");
                   ShouldReboot = true; });

    // Web Server Root URL
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/index.html", "text/html"); });

    // Web Server Root URL
    server->on("/cananalyzer", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/canalyzer.html", "text/html"); });

    server->serveStatic("/", LittleFS, "/");

    // Finally, start the server
    server->begin();
}

void configureGeneralApiEndpoints()
{
    server->on("/api/freestorage", HTTP_GET, [](AsyncWebServerRequest *request)
               { getFsUsagePercent(request); });

    server->on("/api/listfiles", HTTP_GET, [](AsyncWebServerRequest *request)
               {
                   if (request->hasParam("path"))
                   {
                       const String path = request->getParam("path")->value();
                       listFsFiles(request, path);
                   }
                   else
                   {
                       listFsFiles(request);
                   } });

    // Info GET
    server->on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request)
               { getSystemStatus(request); });
}

#pragma region "General Config"

void configureGeneralEndpoints()
{
    server->on("/general", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/general.html", "text/html"); });

    server->on("/api/config/general", HTTP_GET, [](AsyncWebServerRequest *request)
               { getGeneralConfig(request); });

    // General Config POST
    auto *rcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/general",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onGeneralConfigReceive(request, json);
            });

    rcvHandler->setMethod(HTTP_POST);
    server->addHandler(rcvHandler);
}

void getGeneralConfig(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["heatingvalues"] = configuration.Features.HeatingParameters;
    doc["watervalues"] = configuration.Features.WaterParameters;
    doc["auxvalues"] = configuration.Features.AuxiliaryParameters;
    doc["overrideot"] = configuration.Features.UseAuxiliaryOutsideTempReference;
    doc["tz"] = configuration.General.Timezone;
    doc["posix-tz"] = configuration.General.PosixTimezone;
    doc["busmsgtimeout"] = configuration.General.BusMessageTimeout;
    doc["debug"] = configuration.General.Debug;
    doc["sniffing"] = configuration.General.Sniffing;
    doc["failsafe-enabled"] = configuration.FailSafe.Enabled;
    doc["failsafe-timeout"] = configuration.FailSafe.CommandTimeoutSeconds;
    char startTime[6];
    char stopTime[6];
    snprintf(startTime, sizeof(startTime), "%02d:%02d", configuration.FailSafe.StartHour, configuration.FailSafe.StartMinute);
    snprintf(stopTime, sizeof(stopTime), "%02d:%02d", configuration.FailSafe.StopHour, configuration.FailSafe.StopMinute);
    doc["failsafe-start"] = startTime;
    doc["failsafe-stop"] = stopTime;
    doc["failsafe-unknown-time-heat"] = configuration.FailSafe.HeatWhenTimeUnknown;
    doc["failsafe-basepoint"] = configuration.FailSafe.BasepointTemperature;
    doc["failsafe-endpoint"] = configuration.FailSafe.EndpointTemperature;
    doc["failsafe-minimum-feed"] = configuration.FailSafe.MinimumFeedTemperature;
    doc["failsafe-maximum-feed"] = configuration.FailSafe.MaximumFeedTemperature;
    sendJson(doc, request);
}

void onGeneralConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    JsonDocument doc;
    if (json.is<JsonArray>())
    {
        doc = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
        doc = json.as<JsonObject>();
    }

    int failSafeStartHour = configuration.FailSafe.StartHour;
    int failSafeStartMinute = configuration.FailSafe.StartMinute;
    int failSafeStopHour = configuration.FailSafe.StopHour;
    int failSafeStopMinute = configuration.FailSafe.StopMinute;
    unsigned long failSafeTimeout = configuration.FailSafe.CommandTimeoutSeconds;
    double failSafeBasepoint = configuration.FailSafe.BasepointTemperature;
    double failSafeEndpoint = configuration.FailSafe.EndpointTemperature;
    double failSafeMinimum = configuration.FailSafe.MinimumFeedTemperature;
    double failSafeMaximum = configuration.FailSafe.MaximumFeedTemperature;

    auto parseTime = [](const String &value, int &hour, int &minute)
    {
        if (value.length() != 5 || value.charAt(2) != ':')
            return false;
        hour = value.substring(0, 2).toInt();
        minute = value.substring(3, 5).toInt();
        return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
    };

    if (!doc["failsafe-timeout"].isNull())
        failSafeTimeout = doc["failsafe-timeout"].as<unsigned long>();
    if (!doc["failsafe-basepoint"].isNull())
        failSafeBasepoint = doc["failsafe-basepoint"].as<double>();
    if (!doc["failsafe-endpoint"].isNull())
        failSafeEndpoint = doc["failsafe-endpoint"].as<double>();
    if (!doc["failsafe-minimum-feed"].isNull())
        failSafeMinimum = doc["failsafe-minimum-feed"].as<double>();
    if (!doc["failsafe-maximum-feed"].isNull())
        failSafeMaximum = doc["failsafe-maximum-feed"].as<double>();

    const bool startValid = doc["failsafe-start"].isNull() ||
                            parseTime(doc["failsafe-start"].as<String>(), failSafeStartHour, failSafeStartMinute);
    const bool stopValid = doc["failsafe-stop"].isNull() ||
                           parseTime(doc["failsafe-stop"].as<String>(), failSafeStopHour, failSafeStopMinute);
    if (!startValid || !stopValid || failSafeTimeout < 10 || failSafeTimeout > 86400 ||
        !isfinite(failSafeBasepoint) || !isfinite(failSafeEndpoint) ||
        !isfinite(failSafeMinimum) || !isfinite(failSafeMaximum) ||
        failSafeMinimum < 0 || failSafeMaximum > 100 || failSafeMinimum > failSafeMaximum)
    {
        request->send(400, "application/json", R"({"status":400,"msg":"Invalid fail-safe time, timeout, or temperature range."})");
        return;
    }

    if (!doc["heatingvalues"].isNull())
        configuration.Features.HeatingParameters = doc["heatingvalues"] == "true";

    if (!doc["watervalues"].isNull())
        configuration.Features.WaterParameters = doc["watervalues"] == "true";

    if (!doc["auxvalues"].isNull())
        configuration.Features.AuxiliaryParameters = doc["auxvalues"] == "true";

    if (!doc["overrideot"].isNull())
        configuration.Features.UseAuxiliaryOutsideTempReference = doc["overrideot"] == "true";

    if (!doc["tz"].isNull())
        strlcpy(configuration.General.Timezone, doc["tz"], sizeof(configuration.General.Timezone));

    if (!doc["posix-tz"].isNull())
        strlcpy(configuration.General.PosixTimezone, doc["posix-tz"], sizeof(configuration.General.PosixTimezone));

    if (!doc["busmsgtimeout"].isNull())
        configuration.General.BusMessageTimeout = doc["busmsgtimeout"];

    if (!doc["debug"].isNull())
        configuration.General.Debug = doc["debug"] == "true";

    if (!doc["sniffing"].isNull())
        configuration.General.Sniffing = doc["sniffing"] == "true";

    if (!doc["failsafe-enabled"].isNull())
        configuration.FailSafe.Enabled = doc["failsafe-enabled"] == "true";
    if (!doc["failsafe-unknown-time-heat"].isNull())
        configuration.FailSafe.HeatWhenTimeUnknown = doc["failsafe-unknown-time-heat"] == "true";
    configuration.FailSafe.CommandTimeoutSeconds = failSafeTimeout;
    configuration.FailSafe.StartHour = failSafeStartHour;
    configuration.FailSafe.StartMinute = failSafeStartMinute;
    configuration.FailSafe.StopHour = failSafeStopHour;
    configuration.FailSafe.StopMinute = failSafeStopMinute;
    configuration.FailSafe.BasepointTemperature = failSafeBasepoint;
    configuration.FailSafe.EndpointTemperature = failSafeEndpoint;
    configuration.FailSafe.MinimumFeedTemperature = failSafeMinimum;
    configuration.FailSafe.MaximumFeedTemperature = failSafeMaximum;

    configuration.General.Debug = configuration.General.Debug;

    sendConfigurationSaveResult(request, R"({"status":200, "msg":"Feature configuration has been saved."})");
}

#pragma endregion

#pragma region "Wifi Related"

void configureWifiEndpoints()
{
    // WiFi config Page
    server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/wifi.html", "text/html"); });

    // Wifi Networks GET
    server->on("/api/wifi/networks", HTTP_GET, [](AsyncWebServerRequest *request)
               { getWifiNetworks(request); });

    // Wifi Current Connected Network GET
    server->on("/api/wifi/network", HTTP_GET, [](AsyncWebServerRequest *request)
               { getCurrentWifiNetwork(request); });

    // Wifi Config POST
    auto *wifiConfigRcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/wifi",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onWifiConfigReceive(request, json);
            });

    wifiConfigRcvHandler->setMethod(HTTP_POST);
    server->addHandler(wifiConfigRcvHandler);

    // Wifi Config GET
    server->on("/api/config/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
               { getWifiConfig(request); });
}

void getWifiConfig(AsyncWebServerRequest *request)
{

    JsonDocument doc;
    doc["wifi_ssid"] = configuration.Wifi.SSID;
    doc["wifi_pw"] = configuration.Wifi.Password;
    doc["hostname"] = configuration.Wifi.Hostname;
    sendJson(doc, request);
}

void onWifiConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    JsonDocument doc;
    if (json.is<JsonArray>())
    {
        doc = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
        doc = json.as<JsonObject>();
    }

    if (doc["wifi_ssid"].isNull() || doc["wifi_pw"].isNull() || doc["hostname"].isNull())
    {
        request->send(400, "application/json", R"({"status":400, "msg":"Missing field values. Expected fields are: wifi_ssid, wifi_pw, hostname"})");
        return;
    }

    /*
{
    "wifi_ssid": "",
    "wifi_pw": "",
    "hostname": ""
}
*/
    strlcpy(configuration.Wifi.SSID, doc["wifi_ssid"], sizeof(configuration.Wifi.SSID));
    strlcpy(configuration.Wifi.Password, doc["wifi_pw"], sizeof(configuration.Wifi.Password));
    strlcpy(configuration.Wifi.Hostname, doc["hostname"], sizeof(configuration.Wifi.Hostname));
    sendConfigurationSaveResult(request, R"({"status":200, "msg":"Wifi configuration has been saved."})");
}

void getCurrentWifiNetwork(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["dns"] = WiFi.dnsIP().toString();
    doc["mask"] = WiFi.subnetMask().toString();
    doc["channel"] = WiFi.channel();
    sendJson(doc, request);
}

void getWifiNetworks(AsyncWebServerRequest *request)
{
    JsonDocument doc;

    int count = WiFi.scanNetworks();
    if (count == 0)
        sendJson(doc, request);
    for (size_t i = 0; i < count; i++)
    {
        JsonObject network = doc.add<JsonObject>();
        network["SSID"] = WiFi.SSID(i);
        network["RSSI"] = WiFi.RSSI(i);
        network["Encryption"] = WiFi.encryptionType(i);
    }
    sendJson(doc, request);
}

#pragma endregion

#pragma region "MQTT Related"

void configureMqttEndpoints()
{
    // MQTT config Page
    server->on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/mqtt.html", "text/html"); });

    // MQTT Config GET
    server->on("/api/config/mqtt", HTTP_GET, [](AsyncWebServerRequest *request)
               { getMqttConfig(request); });

    // MQTT Config POST
    auto *mqttConfigRcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/mqtt",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onMqttConfigReceive(request, json);
            });

    mqttConfigRcvHandler->setMethod(HTTP_POST);
    server->addHandler(mqttConfigRcvHandler);

    // MQTT Topics GET
    server->on("/api/config/mqtt-topics", HTTP_GET, [](AsyncWebServerRequest *request)
               { getMqttTopicConfig(request); });
    // MQTT Topics POST
    auto *mqttTopicsRcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/mqtt-topics",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onMqttTopicConfigReceive(request, json);
            });

    mqttTopicsRcvHandler->setMethod(HTTP_POST);
    server->addHandler(mqttTopicsRcvHandler);

    server->on("/api/config/homeassistant", HTTP_GET, [](AsyncWebServerRequest *request)
               { getHomeAssistantConfig(request); });

    auto *homeAssistantRcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/homeassistant",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onHomeAssistantConfigReceive(request, json);
            });

    homeAssistantRcvHandler->setMethod(HTTP_POST);
    server->addHandler(homeAssistantRcvHandler);
}

void getMqttConfig(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["mqtt-server"] = configuration.Mqtt.Server;
    doc["mqtt-port"] = configuration.Mqtt.Port;
    doc["mqtt-user"] = configuration.Mqtt.User;
    doc["mqtt-password"] = configuration.Mqtt.Password;

    sendJson(doc, request);
}

void onMqttConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    JsonDocument doc;
    if (json.is<JsonArray>())
    {
        doc = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
        doc = json.as<JsonObject>();
    }

    if (doc["mqtt-server"].isNull() || doc["mqtt-port"].isNull() || doc["mqtt-user"].isNull() || doc["mqtt-password"].isNull())
    {
        request->send(400, "application/json", R"({"status":400, "msg":"Missing field values. Expected fields are: server, port, user, password"})");
        return;
    }

    strlcpy(configuration.Mqtt.Server, doc["mqtt-server"], sizeof(configuration.Mqtt.Server));
    configuration.Mqtt.Port = doc["mqtt-port"];
    strlcpy(configuration.Mqtt.User, doc["mqtt-user"], sizeof(configuration.Mqtt.User));
    strlcpy(configuration.Mqtt.Password, doc["mqtt-password"], sizeof(configuration.Mqtt.Password));
    sendConfigurationSaveResult(request, R"({"status":200, "msg":"MQTT configuration has been saved."})");
}

void getMqttTopicConfig(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["status"] = configuration.Mqtt.Topics.Status;
    doc["statusrequest"] = configuration.Mqtt.Topics.StatusRequest;
    doc["auxvalues"] = configuration.Mqtt.Topics.AuxiliaryValues;
    doc["boost"] = configuration.Mqtt.Topics.Boost;
    doc["fastheatup"] = configuration.Mqtt.Topics.FastHeatup;
    doc["heatingparameters"] = configuration.Mqtt.Topics.HeatingParameters;
    doc["heatingvalues"] = configuration.Mqtt.Topics.HeatingValues;
    doc["waterparameters"] = configuration.Mqtt.Topics.WaterParameters;
    doc["watervalues"] = configuration.Mqtt.Topics.WaterValues;

    sendJson(doc, request);
}

void onMqttTopicConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    JsonDocument doc;
    if (json.is<JsonArray>())
    {
        doc = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
        doc = json.as<JsonObject>();
    }

    if (doc["status"].isNull() ||
        doc["statusrequest"].isNull() ||
        doc["auxvalues"].isNull() ||
        doc["boost"].isNull() ||
        doc["fastheatup"].isNull() ||
        doc["heatingparameters"].isNull() ||
        doc["heatingvalues"].isNull() ||
        doc["waterparameters"].isNull() ||
        doc["watervalues"].isNull())
    {
        request->send(400, "application/json",
                      R"({"status":400, "msg":"Missing field values. Expected fields are: status, statusrequest, auxvalues, boost, fastheaup, heatingvalues, heatingparameters, waterparameters, watervalues"})");
        return;
    }

    strlcpy(configuration.Mqtt.Topics.AuxiliaryValues, doc["auxvalues"], sizeof(configuration.Mqtt.Topics.AuxiliaryValues));
    strlcpy(configuration.Mqtt.Topics.Boost, doc["boost"], sizeof(configuration.Mqtt.Topics.Boost));
    strlcpy(configuration.Mqtt.Topics.FastHeatup, doc["fastheatup"], sizeof(configuration.Mqtt.Topics.FastHeatup));
    strlcpy(configuration.Mqtt.Topics.HeatingParameters, doc["heatingparameters"], sizeof(configuration.Mqtt.Topics.HeatingParameters));
    strlcpy(configuration.Mqtt.Topics.HeatingValues, doc["heatingvalues"], sizeof(configuration.Mqtt.Topics.HeatingValues));
    strlcpy(configuration.Mqtt.Topics.Status, doc["status"], sizeof(configuration.Mqtt.Topics.Status));
    strlcpy(configuration.Mqtt.Topics.StatusRequest, doc["statusrequest"], sizeof(configuration.Mqtt.Topics.StatusRequest));
    strlcpy(configuration.Mqtt.Topics.WaterParameters, doc["waterparameters"], sizeof(configuration.Mqtt.Topics.WaterParameters));
    strlcpy(configuration.Mqtt.Topics.WaterValues, doc["watervalues"], sizeof(configuration.Mqtt.Topics.WaterValues));
    sendConfigurationSaveResult(request, R"({"status":200, "msg":"MQTT Topics have been saved."})");
}

void getHomeAssistantConfig(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["enabled"] = configuration.HomeAssistant.Enabled;
    doc["discovery-prefix"] = configuration.HomeAssistant.AutoDiscoveryPrefix;
    doc["device-id"] = configuration.HomeAssistant.DeviceId;
    doc["temperature-unit"] = configuration.HomeAssistant.TempUnit;
    doc["off-delay"] = configuration.HomeAssistant.OffDelay;
    doc["state-topic"] = configuration.HomeAssistant.StateTopic;
    sendJson(doc, request);
}

void onHomeAssistantConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    if (!json.is<JsonObject>())
    {
        request->send(400, "application/json", R"({"status":400, "msg":"Expected a JSON object."})");
        return;
    }

    JsonObject doc = json.as<JsonObject>();
    if (doc["enabled"].isNull() ||
        doc["discovery-prefix"].isNull() ||
        doc["device-id"].isNull() ||
        doc["temperature-unit"].isNull() ||
        doc["off-delay"].isNull())
    {
        request->send(400, "application/json", R"({"status":400, "msg":"Missing Home Assistant configuration fields."})");
        return;
    }

    String discoveryPrefix = doc["discovery-prefix"].as<String>();
    discoveryPrefix.trim();
    while (discoveryPrefix.endsWith("/"))
        discoveryPrefix.remove(discoveryPrefix.length() - 1);

    String deviceId = doc["device-id"].as<String>();
    deviceId.trim();
    deviceId.replace(" ", "_");
    deviceId.replace("/", "_");

    String temperatureUnit = doc["temperature-unit"].as<String>();
    temperatureUnit.trim();
    const int offDelay = doc["off-delay"].as<int>();

    if (discoveryPrefix.isEmpty() || discoveryPrefix.indexOf('#') >= 0 || discoveryPrefix.indexOf('+') >= 0 ||
        deviceId.isEmpty() || temperatureUnit.isEmpty() || offDelay < 0)
    {
        request->send(400, "application/json", R"({"status":400, "msg":"Invalid discovery prefix, device ID, temperature unit, or off delay."})");
        return;
    }

    const bool previousEnabled = configuration.HomeAssistant.Enabled;
    const String previousDiscoveryPrefix = configuration.HomeAssistant.AutoDiscoveryPrefix;
    const String previousDeviceId = configuration.HomeAssistant.DeviceId;
    const String previousStateTopic = configuration.HomeAssistant.StateTopic;

    bool enabled = false;
    if (doc["enabled"].is<bool>())
        enabled = doc["enabled"].as<bool>();
    else
    {
        String enabledValue = doc["enabled"].as<String>();
        enabledValue.toLowerCase();
        enabled = enabledValue == "true" || enabledValue == "1" || enabledValue == "on";
    }

    configuration.HomeAssistant.Enabled = enabled;
    configuration.HomeAssistant.AutoDiscoveryPrefix = discoveryPrefix;
    configuration.HomeAssistant.DeviceId = deviceId;
    configuration.HomeAssistant.TempUnit = temperatureUnit;
    configuration.HomeAssistant.OffDelay = offDelay;
    configuration.HomeAssistant.StateTopic = "junkerscontrol/" + deviceId + "/";

    if (sendConfigurationSaveResult(request, R"({"status":200, "msg":"Home Assistant configuration has been saved. MQTT will reconnect automatically."})"))
    {
        const bool identityChanged = previousDiscoveryPrefix != discoveryPrefix || previousDeviceId != deviceId;
        if (previousEnabled && (!enabled || identityChanged) && client.connected())
        {
            const String previousAvailabilityTopic = previousStateTopic + "availability";
            const String previousDiscoveryTopic = previousDiscoveryPrefix + "/device/" + previousDeviceId + "/config";
            client.publish(previousAvailabilityTopic.c_str(), "offline", true);
            client.publish(previousDiscoveryTopic.c_str(), "", true);
        }
        client.disconnect();
    }
}

#pragma endregion

#pragma region "Firmware Related"

void configureFirmwareEndpoints()
{
    //-------------------------------------------------------------------------
    // Firmware Update
    server->on(
        "/upload-firmware", HTTP_POST, [](AsyncWebServerRequest *request)
        { request->send(200); },
        handleDoUpdate);

    server->on("/update-firmware", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/firmware.html", "text/html"); });
}

void handleDoUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!index)
    {
        Serial.println("Update");
        // Decide what to update. If the filename contains "littlefs" it's a filesystem image.
        int cmd = (filename.indexOf("littlefs") > -1) ? U_SPIFFS : U_FLASH;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd))
        {
            Update.printError(Serial);
        }
    }

    if (Update.write(data, len) != len)
    {
        Update.printError(Serial);
        JsonDocument doc;
        doc["status"] = 500;
        doc["msg"] = Update.errorString();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    }

    if (final)
    {
        if (!Update.end(true))
        {
            Update.printError(Serial);
            request->send(500, "application/json", R"({"status":500, "msg":"Update has failed. Please retry again after rebooting."})");
        }
        else
        {
            Serial.println("Update complete");
            request->send(200, "application/json", R"({"status":200, "msg":"Update completed. Reboot to apply."})");
        }
    }
}

#pragma endregion

#pragma region "Filemanager Related"

void configureFilemanagerEndpoints()
{
    server->on(
        "/filemanager/upload", HTTP_POST, [](AsyncWebServerRequest *request)
        {
            UploadResult *result = static_cast<UploadResult *>(request->_tempObject);
            if (result == nullptr)
            {
                request->send(500, "application/json", R"({"status":500,"msg":"Upload failed."})");
                return;
            }
            request->send(result->status, "application/json", result->message);
        },
        handleUpload);

    server->on("/api/config/reload", HTTP_POST, [](AsyncWebServerRequest *request)
               {
                   String validationError;
                   if (!validateConfigurationFile(configFileName, validationError))
                   {
                       JsonDocument responseDoc;
                       responseDoc["status"] = 400;
                       responseDoc["msg"] = validationError;
                       String response;
                       serializeJson(responseDoc, response);
                       request->send(400, "application/json", response);
                       return;
                   }

                   // Keep the uploaded file authoritative until the restart.
                   SetConfigurationUploadPending(true);
                   request->send(202, "application/json",
                                 R"({"status":202,"msg":"Configuration validated. Restarting to load it."})");
                   ShouldReboot = true;
               });

    server->on("/filemanager/file", HTTP_GET, [](AsyncWebServerRequest *request)
               {

      if (request->hasParam("name") && request->hasParam("action")) {
        const char *fileName = request->getParam("name")->value().c_str();
        const char *fileAction = request->getParam("action")->value().c_str();
        if (!LittleFS.exists(fileName)) {
          request->send(400, "text/plain", "ERROR: file does not exist");
        } else {
          if (strcmp(fileAction, "download") == 0) {
            request->send(LittleFS, fileName, "application/octet-stream");
          } else if (strcmp(fileAction, "delete") == 0) {
            LittleFS.remove(fileName);
            request->send(200, "text/plain", "Deleted File: " + String(fileName));
          } else {
            request->send(400, "text/plain", "ERROR: invalid action param supplied");
          }
        }
      } else {
        request->send(400, "text/plain", "ERROR: name and action params required");
      } });

    server->on("/filemanager", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/filemanager.html", "text/html"); });
}

void getFsUsagePercent(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    JsonDocument doc;
    double total = LittleFS.totalBytes();
    double used = LittleFS.usedBytes();
    double free = total - used;
    doc["Free"] = free;
    doc["Used"] = used;
    doc["Total"] = total;
    double usedPercent = (used / total) * 100.0;
    double freePercent = (free / total) * 100.0;
    doc["UsedPercent"] = ceil(usedPercent);
    doc["FreePercent"] = ceil(freePercent);

    serializeJson(doc, *response);
    request->send(response);
}

void listFsFiles(AsyncWebServerRequest *request, String path /* = "/" */)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    if (path.isEmpty())
        path = "/";
    if (!LittleFS.exists(path))
    {
        request->send(404, "application/json", R"({ "status":"Path not found","path":")" + path + "\"}");
        return;
    }
    File rootDir = LittleFS.open(path);
    File fsEntry = rootDir.openNextFile();
    JsonDocument doc;
    JsonArray files = doc[(String)rootDir.path()].to<JsonArray>();

    while (fsEntry)
    {
        JsonObject file = files.add<JsonObject>();
        file["Name"] = (String)fsEntry.name();
        file["Size"] = (int)fsEntry.size();
        file["Directory"] = fsEntry.isDirectory();
        fsEntry = rootDir.openNextFile();
    }

    serializeJson(doc, *response);
    request->send(response);
}

// handles uploads to the filserver
void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    Serial.printf("UPLOAD: Index %i Len %i Final %i Filename %s\r\n", index, len, final, filename.c_str());

    if (!index)
    {
        UploadResult *result = static_cast<UploadResult *>(calloc(1, sizeof(UploadResult)));
        request->_tempObject = result;
        if (result == nullptr)
            return;

        result->status = 200;
        result->message = R"({"status":200,"msg":"File uploaded."})";

        String uploadPath = filename.startsWith("/") ? filename : "/" + filename;
        if (uploadPath == configFileName)
            uploadPath = configurationUploadFileName;

        // open the file on first call and store the file handle in the request object
        request->_tempFile = LittleFS.open(uploadPath, FILE_WRITE, true);
        if (!request->_tempFile)
        {
            result->status = 500;
            result->message = R"({"status":500,"msg":"The upload target could not be opened."})";
            return;
        }
    }

    UploadResult *result = static_cast<UploadResult *>(request->_tempObject);
    if (result == nullptr || result->status != 200)
        return;

    if (len && request->_tempFile.write(data, len) != len)
    {
        result->status = 500;
        result->message = R"({"status":500,"msg":"The uploaded file could not be written completely."})";
    }

    if (final)
    {
        // close the file handle as the upload is now done
        request->_tempFile.flush();
        request->_tempFile.close();

        String uploadPath = filename.startsWith("/") ? filename : "/" + filename;
        if (uploadPath != configFileName || result->status != 200)
            return;

        String validationError;
        if (!validateConfigurationFile(configurationUploadFileName, validationError))
        {
            LittleFS.remove(configurationUploadFileName);
            result->status = 400;
            result->message = R"({"status":400,"msg":"Uploaded configuration is invalid or incomplete."})";
            return;
        }

        // Use the same recovery path as normal configuration saves so boot can
        // restore it if power is lost between the two renames.
        const char *uploadBackupFileName = "/configuration.bak";
        LittleFS.remove(uploadBackupFileName);
        if (LittleFS.exists(configFileName) && !LittleFS.rename(configFileName, uploadBackupFileName))
        {
            LittleFS.remove(configurationUploadFileName);
            result->status = 500;
            result->message = R"({"status":500,"msg":"Current configuration could not be backed up."})";
            return;
        }

        if (!LittleFS.rename(configurationUploadFileName, configFileName))
        {
            LittleFS.rename(uploadBackupFileName, configFileName);
            result->status = 500;
            result->message = R"({"status":500,"msg":"Uploaded configuration could not be committed."})";
            return;
        }

        LittleFS.remove(uploadBackupFileName);
        SetConfigurationUploadPending(true);
        result->message = R"({"status":200,"msg":"Configuration uploaded. Reboot to apply it."})";
    }
}

#pragma endregion

#pragma region "CAN Config"

void configureCanConfigEndpoints()
{
    server->on("/canbus", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/canbus.html", "text/html"); });

    server->on("/api/config/canbus", HTTP_GET, [](AsyncWebServerRequest *request)
               { getCanbusConfig(request); });

    // MQTT Topics POST
    auto *rcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/canbus",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onCanbusConfigReceive(request, json);
            });

    rcvHandler->setMethod(HTTP_POST);
    server->addHandler(rcvHandler);
}

void getCanbusConfig(AsyncWebServerRequest *request)
{
    // Take values directly from configuration
    JsonDocument doc;
    JsonObject CAN_Addresses_Controller = doc["Controller"].to<JsonObject>();
    CAN_Addresses_Controller["FlameStatus"] = IntToHex(configuration.CanAddresses.General.FlameLit);
    CAN_Addresses_Controller["Error"] = IntToHex(configuration.CanAddresses.General.Error);
    CAN_Addresses_Controller["DateTime"] = IntToHex(configuration.CanAddresses.General.DateTime);

    JsonObject CAN_Addresses_Heating = doc["Heating"].to<JsonObject>();
    CAN_Addresses_Heating["FeedCurrent"] = IntToHex(configuration.CanAddresses.Heating.FeedCurrent);
    CAN_Addresses_Heating["FeedMax"] = IntToHex(configuration.CanAddresses.Heating.FeedMax);
    CAN_Addresses_Heating["FeedSetpoint"] = IntToHex(configuration.CanAddresses.Heating.FeedSetpoint);
    CAN_Addresses_Heating["OutsideTemperature"] = IntToHex(configuration.CanAddresses.Heating.OutsideTemperature);
    CAN_Addresses_Heating["Pump"] = IntToHex(configuration.CanAddresses.Heating.Pump);
    CAN_Addresses_Heating["Season"] = IntToHex(configuration.CanAddresses.Heating.Season);
    CAN_Addresses_Heating["Operation"] = IntToHex(configuration.CanAddresses.Heating.Operation);
    CAN_Addresses_Heating["Power"] = IntToHex(configuration.CanAddresses.Heating.Power);
    CAN_Addresses_Heating["Mode"] = IntToHex(configuration.CanAddresses.Heating.Mode);
    CAN_Addresses_Heating["Economy"] = IntToHex(configuration.CanAddresses.Heating.Economy);

    JsonObject CAN_Addresses_HotWater = doc["HotWater"].to<JsonObject>();
    CAN_Addresses_HotWater["SetpointTemperature"] = IntToHex(configuration.CanAddresses.HotWater.SetpointTemperature);
    CAN_Addresses_HotWater["MaxTemperature"] = IntToHex(configuration.CanAddresses.HotWater.MaxTemperature);
    CAN_Addresses_HotWater["CurrentTemperature"] = IntToHex(configuration.CanAddresses.HotWater.CurrentTemperature);
    CAN_Addresses_HotWater["Now"] = IntToHex(configuration.CanAddresses.HotWater.Now);
    CAN_Addresses_HotWater["BufferOperation"] = IntToHex(configuration.CanAddresses.HotWater.BufferOperation);
    CAN_Addresses_HotWater["ContinousFlow"]["SetpointTemperature"] = IntToHex(configuration.CanAddresses.HotWater.ContinousFlowSetpointTemperature);

    JsonObject CAN_Addresses_MixedCircuit = doc["MixedCircuit"].to<JsonObject>();
    CAN_Addresses_MixedCircuit["Pump"] = IntToHex(configuration.CanAddresses.MixedCircuit.Pump);
    CAN_Addresses_MixedCircuit["FeedSetpoint"] = IntToHex(configuration.CanAddresses.MixedCircuit.FeedSetpoint);
    CAN_Addresses_MixedCircuit["FeedCurrent"] = IntToHex(configuration.CanAddresses.MixedCircuit.FeedCurrent);
    CAN_Addresses_MixedCircuit["Economy"] = IntToHex(configuration.CanAddresses.MixedCircuit.Economy);

    sendJson(doc, request);
}

void onCanbusConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    JsonDocument doc;
    if (json.is<JsonArray>())
    {
        doc = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
        doc = json.as<JsonObject>();
    }

    // Quartz setting is added at top of the document
    configuration.CanModuleConfig.CAN_Quartz = doc["quartz"];

    // Now what follows is basically the same as in ReadConfiguration @configuration.cpp

    JsonObject CAN_Addresses_Controller = doc["Controller"];
    configuration.CanAddresses.General.FlameLit = convertHexString(CAN_Addresses_Controller["FlameStatus"].as<const char *>()); // "0x209"
    configuration.CanAddresses.General.Error = convertHexString(CAN_Addresses_Controller["Error"].as<const char *>());          // "0x206"
    configuration.CanAddresses.General.DateTime = convertHexString(CAN_Addresses_Controller["DateTime"].as<const char *>());    // "0x256"

    JsonObject CAN_Addresses_Heating = doc["Heating"];
    configuration.CanAddresses.Heating.FeedCurrent = convertHexString(CAN_Addresses_Heating["FeedCurrent"].as<const char *>());               // "0x201"
    configuration.CanAddresses.Heating.FeedMax = convertHexString(CAN_Addresses_Heating["FeedMax"].as<const char *>());                       // "0x200"
    configuration.CanAddresses.Heating.FeedSetpoint = convertHexString(CAN_Addresses_Heating["FeedSetpoint"].as<const char *>());             // "0x252"
    configuration.CanAddresses.Heating.OutsideTemperature = convertHexString(CAN_Addresses_Heating["OutsideTemperature"].as<const char *>()); // "0x207"
    configuration.CanAddresses.Heating.Pump = convertHexString(CAN_Addresses_Heating["Pump"].as<const char *>());                             // "0x20A"
    configuration.CanAddresses.Heating.Season = convertHexString(CAN_Addresses_Heating["Season"].as<const char *>());                         // "0x20C"
    configuration.CanAddresses.Heating.Operation = convertHexString(CAN_Addresses_Heating["Operation"].as<const char *>());                   // "0x250"
    configuration.CanAddresses.Heating.Power = convertHexString(CAN_Addresses_Heating["Power"].as<const char *>());                           // "0x251"
    configuration.CanAddresses.Heating.Mode = convertHexString(CAN_Addresses_Heating["Mode"].as<const char *>());                             // "0x258"
    configuration.CanAddresses.Heating.Economy = convertHexString(CAN_Addresses_Heating["Economy"].as<const char *>());                       // "0x253"

    JsonObject CAN_Addresses_HotWater = doc["HotWater"];
    configuration.CanAddresses.HotWater.SetpointTemperature = convertHexString(CAN_Addresses_HotWater["SetpointTemperature"].as<const char *>()); // "0x203"
    configuration.CanAddresses.HotWater.MaxTemperature = convertHexString(CAN_Addresses_HotWater["MaxTemperature"].as<const char *>());           // "0x204"
    configuration.CanAddresses.HotWater.CurrentTemperature = convertHexString(CAN_Addresses_HotWater["CurrentTemperature"].as<const char *>());   // "0x205"
    configuration.CanAddresses.HotWater.Now = convertHexString(CAN_Addresses_HotWater["Now"].as<const char *>());                                 // "0x254"
    configuration.CanAddresses.HotWater.BufferOperation = convertHexString(CAN_Addresses_HotWater["BufferOperation"].as<const char *>());         // "0x20B"

    configuration
        .CanAddresses
        .HotWater
        .ContinousFlowSetpointTemperature = convertHexString(CAN_Addresses_HotWater["ContinousFlow"]["SetpointTemperature"].as<const char *>()); // "0x255"

    JsonObject CAN_Addresses_MixedCircuit = doc["MixedCircuit"];
    configuration.CanAddresses.MixedCircuit.Pump = convertHexString(CAN_Addresses_MixedCircuit["Pump"].as<const char *>());                 // "0x404"
    configuration.CanAddresses.MixedCircuit.FeedSetpoint = convertHexString(CAN_Addresses_MixedCircuit["FeedSetpoint"].as<const char *>()); // "0x405"
    configuration.CanAddresses.MixedCircuit.FeedCurrent = convertHexString(CAN_Addresses_MixedCircuit["FeedCurrent"].as<const char *>());   // "0x440"
    configuration.CanAddresses.MixedCircuit.Economy = convertHexString(CAN_Addresses_MixedCircuit["Economy"].as<const char *>());           // "0x407"

    sendConfigurationSaveResult(request, R"({"status":200, "msg":"CAN Config have been saved."})");
}

#pragma endregion

#pragma region "External Temperature Sensors"

void configureAuxSensorsEndpoints()
{
    server->on("/auxsensors", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/auxsensors.html", "text/html"); });

    server->on("/api/config/auxsensors", HTTP_GET, [](AsyncWebServerRequest *request)
               { getAuxSensorsConfig(request); });

    // Aux Sensors POST
    auto *rcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/auxsensors",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onAuxSensorsConfigReceive(request, json);
            });

    rcvHandler->setMethod(HTTP_POST);
    server->addHandler(rcvHandler);
}

void getAuxSensorsConfig(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    // Preserve the existing API contract: the sensors array is the first item
    // in the root array. The web UI deployed on existing devices reads it via
    // response.at(0).
    JsonArray root = doc.to<JsonArray>();
    JsonArray AuxiliarySensors_Sensors = root.add<JsonArray>();

    for (size_t i = 0; i < configuration.TemperatureSensors.SensorCount; i++)
    {
        JsonObject sensorEntry = AuxiliarySensors_Sensors.add<JsonObject>();
        Sensor curSensor = configuration.TemperatureSensors.Sensors[i];
        sensorEntry["Label"] = curSensor.Label;
        sensorEntry["IsReturnValue"] = curSensor.UseAsReturnValueReference;
        JsonArray address = sensorEntry["Address"].to<JsonArray>();
        // Device address has a fixed size of 8
        for (unsigned char curAddress : curSensor.Address)
        {
            char byteBuf[5];
            sprintf(byteBuf, "0x%.2X", curAddress);
            address.add(byteBuf);
        }
    }
    sendJson(doc, request);
}

void onAuxSensorsConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    /*
        Expected JSON:
        [
            {
                "Label": "Feed",
                "IsReturnValue": false,
                "Address":         
                [
                    "0x28", "0x76", "0x51", "0x91", "0x42", "0x20", "0x01", "0xE3"
                ]                
            },
            {
                "Label": "Return",
                "IsReturnValue": true,
                "Address":
                [
                    "0x28", "0x6F", "0x9C", "0xF6", "0x42", "0x20", "0x01", "0xF6"
                ]
            },
            {
                "Label": "Exhaust",
                "IsReturnValue": false,
                "Address":
                [
                    "0x28", "0x6D", "0x98", "0xF5", "0x42", "0x20", "0x01", "0x0F"
                ]
            },
            {
                "Label": "Ambient",
                "IsReturnValue": false,
                "Address":
                [
                    "0x28", "0xBF", "0x39", "0x10", "0x42", "0x20", "0x01", "0x93"
                ]
            }
        ]
    */
    JsonDocument doc;
    if (json.is<JsonArray>())
    {
        doc = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
        doc = json.as<JsonObject>();
    }

    int curSensor = 0;
    bool tempReferenceSensorSet = false;

    JsonArray sensorsArray = doc.as<JsonArray>();

    const size_t sensorCount = sensorsArray.size();

    // Resize the Temperature array
    ceraValues.Auxiliary.Temperatures = (float *)malloc(sensorCount * sizeof(float));

    // Set initial values to zero.
    for (size_t i = 0; i < sensorCount; i++)
    {
        ceraValues.Auxiliary.Temperatures[i] = 0.0F;
    }

    // Init Sensors: This might cause trouble if too many sensorsArray are added... we'll have to see where this is going.
    configuration.TemperatureSensors.Sensors = (Sensor *)malloc(sensorCount * sizeof(Sensor));

    // Set the amount of sensorsArray
    configuration.TemperatureSensors.SensorCount = sensorCount;

    for (JsonObject AuxiliarySensors_Sensor : sensorsArray)
    {
        Sensor newSensor{};
        strlcpy(newSensor.Label, AuxiliarySensors_Sensor["Label"], sizeof(newSensor.Label)); // true
        newSensor.UseAsReturnValueReference = AuxiliarySensors_Sensor["IsReturnValue"].as<bool>();
        JsonArray AuxiliarySensors_Sensor_Address = AuxiliarySensors_Sensor["Address"];
        int i = 0;
        for (JsonVariant value : AuxiliarySensors_Sensor_Address)
        {
            byte addrByte = strtoul(value.as<const char *>(), NULL, 16);
            newSensor.Address[i++] = addrByte;
        }
        configuration.TemperatureSensors.Sensors[curSensor++] = newSensor;
        if (configuration.General.Debug)
        {
            if (newSensor.UseAsReturnValueReference && tempReferenceSensorSet)
            {
                Log.printf("\e[0;33mWARN: Sensor #%i is set as temperature reference but another sensor has been already set.\e[0m\r\n", curSensor);
            }
            if (newSensor.UseAsReturnValueReference)
            {
                Log.println("\e[0;36mINFO: The following sensor will be used as a return temperature reference.\e[0m\r\n");
                tempReferenceSensorSet = true;
            }
            Log.printf("\e[0;32mAdded Sensor #%i with Label '%s'\r\n\e[0m", curSensor, newSensor.Label);
        }
    }

    sendConfigurationSaveResult(request, R"({"status":200, "msg":"Auxiliary Sensors have been saved."})");
}

#pragma endregion

#pragma region "LED Configuration"

void configureLedConfigEndpoints()
{
    server->on("/leds", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/leds.html", "text/html"); });

    server->on("/api/config/leds", HTTP_GET, [](AsyncWebServerRequest *request)
               { getLedConfig(request); });

    // Aux Sensors POST
    auto *rcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/leds",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onLedConfigReceive(request, json);
            });

    rcvHandler->setMethod(HTTP_POST);
    server->addHandler(rcvHandler);
}

void getLedConfig(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["wifi-led"] = configuration.LEDs.WifiLed;
    doc["status-led"] = configuration.LEDs.StatusLed;
    doc["mqtt-led"] = configuration.LEDs.MqttLed;
    doc["heating-led"] = configuration.LEDs.HeatingLed;
    sendJson(doc, request);
}

void onLedConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    JsonDocument doc;
    if (json.is<JsonArray>())
    {
        doc = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
        doc = json.as<JsonObject>();
    }

    if (doc["wifi-led"].isNull() || doc["status-led"].isNull() ||
        doc["mqtt-led"].isNull() || doc["heating-led"].isNull())
    {
        request->send(400, "application/json", R"({"status":400,"msg":"All four LED GPIO values are required."})");
        return;
    }

    const int wifiLed = doc["wifi-led"].as<int>();
    const int statusLed = doc["status-led"].as<int>();
    const int mqttLed = doc["mqtt-led"].as<int>();
    const int heatingLed = doc["heating-led"].as<int>();
    if (wifiLed < 0 || wifiLed > 39 || statusLed < 0 || statusLed > 39 ||
        mqttLed < 0 || mqttLed > 39 || heatingLed < 0 || heatingLed > 39)
    {
        request->send(400, "application/json", R"({"status":400,"msg":"LED GPIO values must be between 0 and 39."})");
        return;
    }

    configuration.LEDs.WifiLed = wifiLed;
    configuration.LEDs.StatusLed = statusLed;
    configuration.LEDs.MqttLed = mqttLed;
    configuration.LEDs.HeatingLed = heatingLed;

    sendConfigurationSaveResult(request, R"({"status":200, "msg":"LED config has been saved."})");

}

#pragma endregion

void getSystemStatus(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    doc["cores"] = ESP.getChipCores();
    doc["model"] = ESP.getChipModel();
    doc["revision"] = ESP.getChipRevision();
    doc["frequency"] = ESP.getCpuFreqMHz();
    doc["freeheap"] = ESP.getFreeHeap();
    doc["heap"] = ESP.getHeapSize();
    doc["freesketch"] = ESP.getFreeSketchSpace();
    doc["sketchsize"] = ESP.getSketchSize();
    doc["canstatus"] = CanConfigErrorCode;
    doc["canerrorcount"] = CanSendErrorCount;
    doc["mqtt"] = client.connected();
    doc["failsafe"] = IsFailSafeActive();

    sendJson(doc, request);
}
