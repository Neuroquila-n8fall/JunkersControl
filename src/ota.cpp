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

        String type;
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
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        otaRunning = false;
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });
      
  ArduinoOTA.setHostname(configuration.Wifi.Hostname);
  ArduinoOTA.begin();
}