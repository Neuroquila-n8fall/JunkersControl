#include <Arduino.h>
#include "wifi_config.h"
#include "main.h"
#include "timesync.h"
#include <configuration.h>

//-- WiFi Check interval for status output
const int wifiRetryInterval = 30000;
// Roaming is deliberately conservative: a weak connection must persist before
// a background scan is started, and a new AP must be materially better.
static constexpr int32_t wifiRoamRssiThreshold = -75;
static constexpr int32_t wifiRoamMinimumCandidateRssi = -70;
static constexpr int32_t wifiRoamMinimumImprovement = 12;
static constexpr unsigned long wifiRoamRssiCheckInterval = 10000;
static constexpr unsigned long wifiRoamLowRssiHoldTime = 30000;
static constexpr unsigned long wifiRoamCooldown = 300000;
static constexpr uint32_t wifiRoamScanTimePerChannel = 120;
//-- Wifi Client object
WiFiClient espClient;
static unsigned long lastWifiAttempt = 0;
static bool wifiWasConnected = false;
static unsigned long lastWifiRssiCheck = 0;
static unsigned long lowWifiRssiSince = 0;
static unsigned long lastWifiRoamAttempt = 0;
static volatile bool wifiRoamScanActive = false;
static volatile bool manualWifiScanActive = false;
static int32_t wifiRoamSourceRssi = 0;
static uint8_t wifiRoamSourceBssid[6] = {};

static void completeWifiRoamScan()
{
  const int16_t scanResult = WiFi.scanComplete();
  if (scanResult == WIFI_SCAN_RUNNING)
    return;

  wifiRoamScanActive = false;

  if (scanResult == WIFI_SCAN_FAILED)
  {
    Serial.println("WiFi roaming scan failed.");
    return;
  }

  int32_t bestRssi = INT32_MIN;
  int32_t bestChannel = 0;
  uint8_t bestBssid[6] = {};
  String bestBssidText;

  for (int16_t i = 0; i < scanResult; i++)
  {
    if (WiFi.SSID(i) != configuration.Wifi.SSID)
      continue;

    uint8_t candidateBssid[6];
    if (WiFi.BSSID(i, candidateBssid) == nullptr ||
        memcmp(candidateBssid, wifiRoamSourceBssid, sizeof(candidateBssid)) == 0)
      continue;

    const int32_t candidateRssi = WiFi.RSSI(i);
    if (candidateRssi < wifiRoamMinimumCandidateRssi ||
        candidateRssi < wifiRoamSourceRssi + wifiRoamMinimumImprovement ||
        candidateRssi <= bestRssi)
      continue;

    bestRssi = candidateRssi;
    bestChannel = WiFi.channel(i);
    memcpy(bestBssid, candidateBssid, sizeof(bestBssid));
    bestBssidText = WiFi.BSSIDstr(i);
  }

  WiFi.scanDelete();

  if (bestChannel == 0 || !WiFi.isConnected())
  {
    if (configuration.General.Debug)
      Serial.printf("No better WiFi AP found (current RSSI: %ld dBm).\r\n", static_cast<long>(wifiRoamSourceRssi));
    return;
  }

  Serial.printf("Roaming WiFi from %ld dBm to %s at %ld dBm on channel %ld.\r\n",
                static_cast<long>(wifiRoamSourceRssi), bestBssidText.c_str(),
                static_cast<long>(bestRssi), static_cast<long>(bestChannel));

  // This is an intentional, short break-before-make handover. begin() returns
  // immediately, leaving the networking task to perform the connection while
  // the main loop continues servicing CAN.
  WiFi.disconnect(false, 0);
  WiFi.begin(configuration.Wifi.SSID, configuration.Wifi.Password,
             bestChannel, bestBssid, true);
  lastWifiAttempt = millis();
  wifiWasConnected = false;
}

static void updateWifiRoaming()
{
  if (wifiRoamScanActive)
  {
    completeWifiRoamScan();
    return;
  }

  if (manualWifiScanActive)
    return;

  const unsigned long now = millis();
  if (now - lastWifiRssiCheck < wifiRoamRssiCheckInterval)
    return;
  lastWifiRssiCheck = now;

  const int32_t currentRssi = WiFi.RSSI();
  if (currentRssi >= wifiRoamRssiThreshold)
  {
    lowWifiRssiSince = 0;
    return;
  }

  if (lowWifiRssiSince == 0)
  {
    lowWifiRssiSince = now;
    return;
  }

  if (now - lowWifiRssiSince < wifiRoamLowRssiHoldTime ||
      (lastWifiRoamAttempt != 0 && now - lastWifiRoamAttempt < wifiRoamCooldown))
    return;

  uint8_t *currentBssid = WiFi.BSSID(wifiRoamSourceBssid);
  if (currentBssid == nullptr)
    return;

  wifiRoamSourceRssi = currentRssi;
  lastWifiRoamAttempt = now;
  lowWifiRssiSince = 0;

  const int16_t result = WiFi.scanNetworks(true, false, false,
                                            wifiRoamScanTimePerChannel, 0,
                                            configuration.Wifi.SSID);
  if (result == WIFI_SCAN_RUNNING)
  {
    wifiRoamScanActive = true;
    if (configuration.General.Debug)
      Serial.printf("Weak WiFi signal (%ld dBm). Scanning for a better AP.\r\n",
                    static_cast<long>(currentRssi));
    return;
  }

  Serial.printf("Unable to start WiFi roaming scan (result %d).\r\n", result);
}

bool beginManualWifiScan()
{
  if (wifiRoamScanActive || manualWifiScanActive)
    return false;

  manualWifiScanActive = true;
  return true;
}

void endManualWifiScan()
{
  manualWifiScanActive = false;
}


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
    updateWifiRoaming();
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
    // The Arduino default is WIFI_FAST_SCAN, which accepts the first matching
    // SSID it encounters. Scan all channels so RSSI sorting can select the
    // strongest matching AP on boot and after an ordinary reconnection.
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

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
