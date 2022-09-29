#include <webconfig.h>

AsyncWebServer *server;

volatile bool ShouldReboot = false;

void StartApMode()
{
    // MAke sure we're disconnected
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(configuration.Wifi.Hostname, NULL);
    Serial.printf("\e[1;32mWiFi AP launched. Find me @ %s\r\n\e[0m", WiFi.softAPIP().toString());
}

void notFound(AsyncWebServerRequest *request) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);
  request->send(404, "text/plain", "Not found");
}

String getFsUsagePercent()
{
    StaticJsonDocument<128> doc;
    double total = SPIFFS.totalBytes();
    double used = SPIFFS.usedBytes();
    double free = total - used;
    doc["Free"] = free;
    doc["Used"] = used;
    doc["Total"] = total;
    double usedPercent = (used / total) * 100.0;
    double freePercent = (free / total) * 100.0;
    Serial.printf("Calculated SPIFFS Space: Percent free: %.2f Percent used: %.2f", freePercent, usedPercent);
    doc["UsedPercent"] = ceil(usedPercent);
    doc["FreePercent"] = ceil(freePercent);
    String result;
    serializeJson(doc, result);

    return result;
}

void ConfigureAndStartWebserver()
{
    server = new AsyncWebServer(80);

    server->onNotFound(notFound);

    //-------------------------------------------------------------------------
    // File Manager

    server->onFileUpload(handleUpload);

    server->on("/filemanager", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(SPIFFS, "/frontend/filemanager.html", "text/html", false, processor); });

    server->on("/api/freestorage", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(200, "application/json", getFsUsagePercent()); });

    server->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(200, "text/plain", listFiles(true)); });

    server->on("/file", HTTP_GET, [](AsyncWebServerRequest *request)
               {

      if (request->hasParam("name") && request->hasParam("action")) {
        const char *fileName = request->getParam("name")->value().c_str();
        const char *fileAction = request->getParam("action")->value().c_str();
        if (!SPIFFS.exists(fileName)) {
          request->send(400, "text/plain", "ERROR: file does not exist");
        } else {
          if (strcmp(fileAction, "download") == 0) {
            request->send(SPIFFS, fileName, "application/octet-stream");
          } else if (strcmp(fileAction, "delete") == 0) {
            SPIFFS.remove(fileName);
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
               { Log.println("Server Accessed");
                request->send(SPIFFS, "/frontend/index.html", "text/html"); });

    // WiFi config
    server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(SPIFFS, "/frontend/wifi.html", "text/html"); });

    // Process WiFi input
    server->on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request)
               {
        int params = request->params();
        for (int i = 0; i < params; i++)
        {
            AsyncWebParameter *p = request->getParam(i);
            if (p->isPost())
            {
                // HTTP POST ssid value
                if (p->name() == "wifi_ssid")
                {
                    p->value().toCharArray(configuration.Wifi.SSID, 255);
                }
                // HTTP POST pass value
                if (p->name() == "wifi_pw")
                {
                    p->value().toCharArray(configuration.Wifi.Password, 255);
                }
                // HTTP POST ip value
                if (p->name() == "hostname")
                {
                    p->value().toCharArray(configuration.Wifi.Hostname, 255);
                }
            }
        } });

    server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

      request->send(SPIFFS, "/frontend/reboot.html", "text/html");
      ShouldReboot = true;

  });

    server->serveStatic("/", SPIFFS, "/");

    // Finally, start the server
    server->begin();
}

String processor(const String &var)
{
    if (var == "FREESPIFFS")
    {
        return humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes()));
    }

    if (var == "USEDSPIFFS")
    {
        return humanReadableSize(SPIFFS.usedBytes());
    }

    if (var == "TOTALSPIFFS")
    {
        return humanReadableSize(SPIFFS.totalBytes());
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
    File root = SPIFFS.open("/");
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
        request->_tempFile = SPIFFS.open("/" + filename, FILE_WRITE, true);
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