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
  //(re)connect WiFi
  if (!WiFi.isConnected())
  {
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    //NOTE: This results in 255.255.255 for ALL addresses and has been removed until the issue has been resolved.
    //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // Workaround: makes setHostname() work
    WiFi.setHostname(configuration.Wifi.Hostname);
    log_w("WiFi not connected. Reconnecting...");

    if(Debug)
    log_v("Connecting to %s using password %s and hostname %s", configuration.Wifi.SSID, configuration.Wifi.Password, configuration.Wifi.Hostname);

    WiFi.begin(configuration.Wifi.SSID, configuration.Wifi.Password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      log_e("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }

    if(Debug)
    printWifiStatus();

  }

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
  if (WiFi.isConnected())
    {
      log_i("-------------------------------");
      log_i("Wifi Connected");
      log_i("SSID:\t%s",WiFi.SSID().c_str());
      log_i("IP Address:\t%s",WiFi.localIP().toString());
      log_i("Mask:\t%s", WiFi.subnetMask().toString());
      log_i("Gateway:\t%s", WiFi.gatewayIP().toString());
      log_i("RSSI:\t%idb", WiFi.RSSI());

      myTZ.setLocation(F("Europe/Berlin"));
      log_i("Time: [%s]", myTZ.dateTime().c_str());
      log_i("-------------------------------");
    }
}