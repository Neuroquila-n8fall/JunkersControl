#include <Arduino.h>
#include "wifi_config.h"
#include "main.h"
#include "timesync.h"
#include <configuration.h>

//-- WiFi Check interval for status output
const int wifiRetryInterval = 30000;
//-- Wifi Client object
WiFiClient espClient;
static unsigned long lastWifiAttempt = 0;
static bool wifiWasConnected = false;


void connectWifi()
{
  if (strlen(configuration.Wifi.SSID) == 0 && strlen(configuration.Wifi.Password) == 0)
  {
    Serial.println("Invalid WiFi configuration. Launching AP mode.");
    SetupMode = true;
    StartApMode();
    return;
  }
  const bool connected = WiFi.isConnected();
  if (connected)
  {
    if (!wifiWasConnected)
    {
      wifiWasConnected = true;
      Serial.printf("WiFi connected. Address: %s\r\n", WiFi.localIP().toString().c_str());
      if (!MDNS.begin(configuration.Wifi.Hostname))
        Serial.println("mDNS responder could not be started.");
    }

    SyncTimeIfRequired();
  }
  else if (!SetupMode)
  {
    if (wifiWasConnected)
      Serial.println("WiFi connection lost. Heating control remains active while reconnecting.");
    wifiWasConnected = false;

    const unsigned long now = millis();
    if (lastWifiAttempt != 0 && now - lastWifiAttempt < wifiRetryInterval)
      return;
    lastWifiAttempt = now;

    // Start a connection attempt and return immediately. WiFi reconnects in
    // the ESP networking task while the main loop continues servicing CAN.
    WiFi.disconnect(true, false);
    WiFi.setHostname(configuration.Wifi.Hostname);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.println("WiFi not connected. Reconnecting...");

    if(configuration.General.Debug) {
      Serial.printf("Connecting to %s using hostname %s\r\n", configuration.Wifi.SSID,
                    configuration.Wifi.Hostname);
    }

    WiFi.begin(configuration.Wifi.SSID, configuration.Wifi.Password);
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
  if (WiFi.isConnected() && configuration.General.Debug)
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

      Serial.printf("Time: [%s]\r\n", myTZ.dateTime().c_str());
    }
}
