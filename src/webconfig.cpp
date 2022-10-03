#include <webconfig.h>

AsyncWebServer *server;

volatile bool ShouldReboot = false;
static size_t content_len;

void StartApMode()
{
    // Make sure we're disconnected
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(configuration.Wifi.Hostname, NULL);
    Serial.printf("\e[1;32mWiFi AP launched. Find me @ %s\r\n\e[0m", WiFi.softAPIP().toString());
}

void notFound(AsyncWebServerRequest *request)
{
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    request->send(404, "text/plain", "Not found");
}

void getFsUsagePercent(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    StaticJsonDocument<128> doc;
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
        request->send(404, "application/json", "{ \"status\":\"Path not found\",\"path\":\"" + path + "\"}");
        return;
    }
    File rootDir = LittleFS.open(path);
    File fsEntry = rootDir.openNextFile();
    StaticJsonDocument<2048> doc;
    JsonArray files = doc.createNestedArray((String)rootDir.path());

    while (fsEntry)
    {
        JsonObject file = files.createNestedObject();
        file["Name"] = (String)fsEntry.name();
        file["Size"] = (int)fsEntry.size();
        file["Directory"] = fsEntry.isDirectory();
        fsEntry = rootDir.openNextFile();
    }

    serializeJson(doc, *response);
    request->send(response);
}

void getWifiConfig(AsyncWebServerRequest *request)
{

    StaticJsonDocument<256> doc;
    doc["wifi_ssid"] = configuration.Wifi.SSID;
    doc["wifi_pw"] = configuration.Wifi.Password;
    doc["hostname"] = configuration.Wifi.Hostname;
    sendJson(doc, request);
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

    server->onNotFound(notFound);

    //-------------------------------------------------------------------------
    // Firmware Update
    server->on(
        "/upload-firmware", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
      }, handleDoUpdate);

    server->on("/update-firmware", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/firmware.html", "text/html"); });


    //-------------------------------------------------------------------------
    // File Manager
    
    server->on("/filemanager/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
      }, handleUpload);

    server->on("/filemanager", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/filemanager.html", "text/html", false, processor); });

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

    server->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(200, "text/plain", listFiles(true)); });

    server->on("/file", HTTP_GET, [](AsyncWebServerRequest *request)
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

    // Web Server Root URL
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/index.html", "text/html"); });

    // Info GET
    server->on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request)
               { getSystemStatus(request); });

    // MQTT config Page
    server->on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/frontend/mqtt.html", "text/html"); });

    // MQTT Config GET
    server->on("/api/config/mqtt", HTTP_GET, [](AsyncWebServerRequest *request)
               { getMqttConfig(request); });

    // MQTT Config POST
    AsyncCallbackJsonWebHandler *mqttConfigRcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/mqtt",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onMqttConfigReceive(request, json);
            });

    server->addHandler(mqttConfigRcvHandler);

    // MQTT Topics GET
    server->on("/api/config/mqtt-topics", HTTP_GET, [](AsyncWebServerRequest *request)
               { getMqttTopicConfig(request); });
    // MQTT Topics POST
    AsyncCallbackJsonWebHandler *mqttTopicsRcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/mqtt-topics",
            [](AsyncWebServerRequest *request, JsonVariant &json)
            {
                onMqttTopicConfigReceive(request, json);
            });

    server->addHandler(mqttTopicsRcvHandler);

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
    AsyncCallbackJsonWebHandler *wifiConfigRcvHandler =
        new AsyncCallbackJsonWebHandler(
            "/api/config/wifi",
             [](AsyncWebServerRequest *request, JsonVariant &json) {
                onWifiConfigReceive(request, json);
             });

    server->addHandler(wifiConfigRcvHandler);

    // Wifi Config GET
    server->on("/api/config/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
               { getWifiConfig(request); });

    server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
               {
                   String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

                   request->send(LittleFS, "/frontend/reboot.html", "text/html");
                   ShouldReboot = true; });

    server->serveStatic("/", LittleFS, "/");

    // Finally, start the server
    server->begin();
}

void onWifiConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    StaticJsonDocument<200> doc;
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
        request->send(400, "application/json", "{\"status\":400, \"msg\":\"Missing field values. Expected fields are: wifi_ssid, wifi_pw, hostname\"}");
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
    WriteConfiguration();

    request->send(200, "application/json", "{\"status\":200, \"msg\":\"Wifi configuration has been saved.\"}");
}

void getCurrentWifiNetwork(AsyncWebServerRequest *request)
{
    StaticJsonDocument<512> doc;
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
    StaticJsonDocument<1024> doc;

    int count = WiFi.scanNetworks();
    if (count == 0)
        sendJson(doc, request);
    for (size_t i = 0; i < count; i++)
    {
        JsonObject network = doc.createNestedObject();
        network["SSID"] = WiFi.SSID(i);
        network["RSSI"] = WiFi.RSSI(i);
        network["Encryption"] = WiFi.encryptionType(i);
    }
    sendJson(doc, request);
}

void getMqttConfig(AsyncWebServerRequest *request)
{
    StaticJsonDocument<1024> doc;
    doc["mqtt-server"] = configuration.Mqtt.Server;
    doc["mqtt-port"] = configuration.Mqtt.Port;
    doc["mqtt-user"] = configuration.Mqtt.User;
    doc["mqtt-password"] = configuration.Mqtt.Password;

    sendJson(doc, request);
}

void onMqttConfigReceive(AsyncWebServerRequest *request, JsonVariant &json)
{
    StaticJsonDocument<200> doc;
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
        request->send(400, "application/json", "{\"status\":400, \"msg\":\"Missing field values. Expected fields are: server, port, user, password\"}");
        return;
    }

    strlcpy(configuration.Mqtt.Server, doc["mqtt-server"], sizeof(configuration.Mqtt.Server));
    configuration.Mqtt.Port = doc["mqtt-port"];
    strlcpy(configuration.Mqtt.User, doc["mqtt-user"], sizeof(configuration.Mqtt.User));
    strlcpy(configuration.Mqtt.Password, doc["mqtt-password"], sizeof(configuration.Mqtt.Password));
    WriteConfiguration();

    request->send(200, "application/json", "{\"status\":200, \"msg\":\"MQTT configuration has been saved.\"}");
}

void getMqttTopicConfig(AsyncWebServerRequest *request)
{
    StaticJsonDocument<1024> doc;
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
    StaticJsonDocument<200> doc;
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
                      "{\"status\":400, \"msg\":\"Missing field values. Expected fields are: status, statusrequest, auxvalues, boost, fastheaup, heatingvalues, heatingparameters, waterparameters, watervalues\"}");
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
    WriteConfiguration();

    request->send(200, "application/json", "{\"status\":200, \"msg\":\"MQTT Topics have been saved.\"}");
}

void getSystemStatus(AsyncWebServerRequest *request)
{
    StaticJsonDocument<1024> doc;
    doc["cores"] = ESP.getChipCores();
    doc["model"] = ESP.getChipModel();
    doc["revision"] = ESP.getChipRevision();
    doc["frequency"] = ESP.getCpuFreqMHz();
    doc["freeheap"] = ESP.getFreeHeap();
    doc["heap"] = ESP.getHeapSize();
    doc["freesketch"] = ESP.getFreeSketchSpace();
    doc["sketchsize"] = ESP.getSketchSize();
    doc["canstatus"] = CanSendErrorCount == 0;
    doc["canerrorcount"] = CanSendErrorCount;
    doc["mqtt"] = client.connected();

    sendJson(doc, request);
}

/// @brief Process Literals inside webpages (i.e. %TOTALSPIFFS%) into processed text
/// @param var
/// @return
String processor(const String &var)
{
    if (var == "FREESPIFFS")
    {
        return humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes()));
    }

    if (var == "USEDSPIFFS")
    {
        return humanReadableSize(LittleFS.usedBytes());
    }

    if (var == "TOTALSPIFFS")
    {
        return humanReadableSize(LittleFS.totalBytes());
    }

    return "";
}

// Make size of files human readable
String humanReadableSize(const size_t bytes)
{
    if (bytes < 1024)
        return String(bytes) + " B";
    else if (bytes < (1024 * 1024))
        return String(bytes / 1024.0) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        return String(bytes / 1024.0 / 1024.0) + " MB";
    else
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

// list all of the files, if ishtml=true, return html rather than simple text
String listFiles(bool ishtml)
{
    String returnText = "";
    File root = LittleFS.open("/");
    File foundfile = root.openNextFile();
    if (ishtml)
    {
        returnText += "<table class=\"table\"><thead><tr><th scope=\"col\">Name</th><th scope=\"col\">Size</th><th></th><th></th></tr></thead>";
        returnText += "<tbody class=\"table-group-divider\">";
    }
    while (foundfile)
    {
        if (ishtml)
        {
            returnText += "<tr><td>" + String(foundfile.name()) + "</td><td>" + humanReadableSize(foundfile.size()) + "</td>";
            returnText += "<td><button class=\"btn btn-primary\" onclick=\"downloadDeleteButton(\'/" + String(foundfile.name()) + "\', \'download\')\">Download</button>";
            returnText += "<td><button class=\"ml-2 btn btn-danger\" onclick=\"downloadDeleteButton(\'/" + String(foundfile.name()) + "\', \'delete\')\">Delete</button></tr>";
        }
        else
        {
            returnText += "File: " + String(foundfile.name()) + " Size: " + humanReadableSize(foundfile.size()) + "\n";
        }
        foundfile = root.openNextFile();
    }
    if (ishtml)
    {
        returnText += "</tbody></table>";
    }
    root.close();
    foundfile.close();
    return returnText;
}

// handles uploads to the filserver
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    Serial.printf("UPLOAD: Index %i Len %i Final %i Filename %s\r\n", index, len, final, filename);

    if (!index)
    {
        // open the file on first call and store the file handle in the request object
        request->_tempFile = LittleFS.open(filename, FILE_WRITE, true);
    }

    if (len)
    {
        // stream the incoming chunk to the opened file
        request->_tempFile.write(data, len);
    }

    if (final)
    {
        // close the file handle as the upload is now done
        request->_tempFile.close();
    }
}

void handleDoUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!index)
    {
        Serial.println("Update");
        content_len = request->contentLength();
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
        StaticJsonDocument<512> doc;
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
            request->send(500, "application/json", "{\"status\":500, \"msg\":\"Update has failed. Please retry again after rebooting.\"}");
        }
        else
        {
            Serial.println("Update complete");
            request->send(200, "application/json", "{\"status\":200, \"msg\":\"Update completed. Reboot to apply.\"}");
        }
    }
}