#include <Arduino.h>
#include "wifi_config.h"
#include "main.h"
//#include "arduino_secrets.h"
#include "timesync.h"
#include <configuration.h>

//——————————————————————————————————————————————————————————————————————————————
//  WiFi Settings
//——————————————————————————————————————————————————————————————————————————————
//const char *ssid = SECRET_SSID;
//const char *pass = SECRET_PASS;
const char hostName[] = "FXHEATCTRL01";
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
    WiFi.setHostname(configuration.Wifi_Hostname);
    Serial.println("WiFi not connected. Reconnecting...");
    Serial.printf("Connecting to %s using password %s and hostname %s \r\n", configuration.Wifi_SSID, configuration.Wifi_Password, configuration.Wifi_Hostname);
    WiFi.begin(configuration.Wifi_SSID, configuration.Wifi_Password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }
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
  if (WiFi.isConnected())
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

      myTZ.setLocation(F("Europe/Berlin"));
      Serial.printf("Time: [%s]\r\n", myTZ.dateTime().c_str());
    }
}