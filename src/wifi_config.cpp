#include <Arduino.h>
#include "wifi_config.h"
#include "main.h"
#include "timesync.h"
#include <configuration.h>

//-- WiFi Check interval for status output
const int wifiRetryInterval = 30000;
//-- Wifi Client object
WiFiClient espClient;


void connectWifi()
{
  if (configuration.Wifi.SSID == NULL && configuration.Wifi.Password == NULL)
  {
    SetupMode = true;
    StartApMode();
    return;
  }
  //(re)connect WiFi if:
  //      - We are in STATION mode and not connected
  //      - We are not connected and not in SetupMode
  if (!WiFi.isConnected() && WiFi.getMode() == WIFI_MODE_STA || !WiFi.isConnected() && !SetupMode )
  {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    //NOTE: This results in 255.255.255 for ALL addresses and has been removed until the issue has been resolved.
    //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // Workaround: makes setHostname() work
    WiFi.setHostname(configuration.Wifi.Hostname);
    Serial.println("WiFi not connected. Reconnecting...");

    if(DebugMode)
    Serial.printf("Connecting to %s using password %s and hostname %s \r\n", configuration.Wifi.SSID, configuration.Wifi.Password, configuration.Wifi.Hostname);
    unsigned long prevConnectMillis = millis();
    WiFi.begin(configuration.Wifi.SSID, configuration.Wifi.Password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      // WAit 10 seconds to connect
      if (millis() - prevConnectMillis >= 10000)
      {
        Serial.println("Connection Failed! Rebooting...");
        ESP.restart();
      }      
    }

    if(DebugMode)
    printWifiStatus();

  }

  //Sync time
  SyncTimeIfRequired();

  unsigned long currentMillis = millis();
  //Print out WiFi Status
  if (currentMillis - wifiConnectMillis >= wifiRetryInterval)
  {
    wifiConnectMillis = currentMillis;
    printWifiStatus();
  }
}

void printWifiStatus()
{
  if (WiFi.isConnected() && DebugMode)
    {
      Serial.println("-------------------------------");
      Serial.println("Wifi Connected");
      Serial.print("SSID:\t");
      Serial.println(WiFi.SSID());
      Serial.print("IP Address:\t");
      Serial.println(WiFi.localIP());
      Serial.print("Mask:\t\t");
      Serial.println(WiFi.subnetMask());
      Serial.print("Gateway:\t");
      Serial.println(WiFi.gatewayIP());
      Serial.print("RSSI:\t\t");
      Serial.println(WiFi.RSSI());

      myTZ.setLocation(configuration.General.Timezone);
      Serial.printf("Time: [%s]\r\n", myTZ.dateTime().c_str());
    }
}