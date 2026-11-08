#include <ota.h>

//-- OTA Flag
bool otaRunning = false;

//Enables OTA Updates
void ota()
{
  ArduinoOTA
      .onStart([]() {
        otaRunning = true;
        //End all "addons"
        TelnetServer.end();
        can.end();
        client.disconnect();

        const char *type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
          type = "sketch";
        }
        else // U_SPIFFS
        {
          type = "filesystem";
          LittleFS.end();
        }

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using LittleFS.end()
        LogInfo("OTA: updating %s", type);
      })
      .onEnd([]() {
        otaRunning = false;
        LogInfo("OTA: update complete ✓");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        // Log every 25 % only; each line is a stored entry, not an overwritten progress bar.
        static unsigned int lastQuarter = 0;
        const unsigned int quarter = (progress / (total / 100)) / 25;
        if (quarter != lastQuarter)
          LogInfo("OTA: %u%%", quarter * 25);
        lastQuarter = quarter;
      })
      .onError([](ota_error_t error) {
        const char *reason = error == OTA_AUTH_ERROR      ? "auth failed"
                             : error == OTA_BEGIN_ERROR   ? "begin failed"
                             : error == OTA_CONNECT_ERROR ? "connect failed"
                             : error == OTA_RECEIVE_ERROR ? "receive failed"
                             : error == OTA_END_ERROR     ? "end failed"
                                                          : "unknown error";
        LogError("OTA: %s ✗ (error %u)", reason, static_cast<unsigned int>(error));
      });
      
  ArduinoOTA.setHostname(configuration.Wifi.Hostname);
  ArduinoOTA.begin();
}