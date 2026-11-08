#include <Arduino.h>
#include "wifi_config.h"
#include "main.h"
#include "logger.h"
#include "timesync.h"
#include <configuration.h>

//-- WiFi Check interval for status output
const int wifiRetryInterval = 30000;
// Roaming: a weak connection must persist before a background scan is started.
// A candidate AP must be clearly better in two consecutive scans, because a
// single scan reading is noisy and the AP may hear us differently than we hear it.
static constexpr int32_t wifiRoamRssiThreshold = -75;
static constexpr int32_t wifiRoamMinimumImprovement = 8;
static constexpr unsigned long wifiRoamRssiCheckInterval = 10000;
static constexpr unsigned long wifiRoamLowRssiHoldTime = 30000;
static constexpr unsigned long wifiRoamCooldown = 300000;
static constexpr uint32_t wifiRoamScanTimePerChannel = 120;
// After a reconnect (not boot, not our own handover) the strongest AP may have
// been missing during the reconnect scan, e.g. while it restarted. Check once
// shortly afterwards whether a mediocre link can be improved.
static constexpr unsigned long wifiRoamPostReconnectDelay = 60000;
static constexpr int32_t wifiRoamPostReconnectRssiThreshold = -67;
//-- Wifi Client object
WiFiClient espClient;
static unsigned long lastWifiAttempt = 0;
static bool wifiWasConnected = false;
static bool wifiConnectedBefore = false;
static bool wifiRoamHandoverPending = false;
static bool wifiPostReconnectScanPending = false;
static unsigned long wifiReconnectedAt = 0;
static unsigned long lastWifiRssiCheck = 0;
static unsigned long lowWifiRssiSince = 0;
static unsigned long lastWifiRoamAttempt = 0;
static volatile bool wifiRoamScanActive = false;
static volatile bool manualWifiScanActive = false;
static int32_t wifiRoamSourceRssi = 0;
static uint8_t wifiRoamSourceBssid[6] = {};
static bool wifiRoamConfirmPending = false;
static uint8_t wifiRoamCandidateBssid[6] = {};

static void completeWifiRoamScan()
{
  const int16_t scanResult = WiFi.scanComplete();
  if (scanResult == WIFI_SCAN_RUNNING)
    return;

  wifiRoamScanActive = false;

  if (scanResult == WIFI_SCAN_FAILED)
  {
    LogWarn("WiFi: roam scan failed ✗");
    wifiRoamConfirmPending = false;
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

    const uint8_t *candidateBssid = WiFi.BSSID(i);
    if (candidateBssid == nullptr ||
        memcmp(candidateBssid, wifiRoamSourceBssid, sizeof(bestBssid)) == 0)
      continue;

    const int32_t candidateRssi = WiFi.RSSI(i);
    if (candidateRssi <= bestRssi)
      continue;

    bestRssi = candidateRssi;
    bestChannel = WiFi.channel(i);
    memcpy(bestBssid, candidateBssid, sizeof(bestBssid));
    bestBssidText = WiFi.BSSIDstr(i);
  }

  WiFi.scanDelete();

  const bool confirming = wifiRoamConfirmPending;
  wifiRoamConfirmPending = false;

  if (bestChannel == 0)
  {
    LogInfo("WiFi: roam scan found no other AP (current %ld dBm)",
            static_cast<long>(wifiRoamSourceRssi));
    return;
  }

  if (bestRssi < wifiRoamSourceRssi + wifiRoamMinimumImprovement)
  {
    LogInfo("WiFi: best other AP %s at %ld dBm, needs +%ld dB over current %ld dBm",
            bestBssidText.c_str(), static_cast<long>(bestRssi),
            static_cast<long>(wifiRoamMinimumImprovement),
            static_cast<long>(wifiRoamSourceRssi));
    return;
  }

  if (!confirming || memcmp(bestBssid, wifiRoamCandidateBssid, sizeof(bestBssid)) != 0)
  {
    memcpy(wifiRoamCandidateBssid, bestBssid, sizeof(bestBssid));
    wifiRoamConfirmPending = true;
    LogInfo("WiFi: roam candidate %s at %ld dBm (current %ld dBm), ⟳ confirming with second scan",
            bestBssidText.c_str(), static_cast<long>(bestRssi),
            static_cast<long>(wifiRoamSourceRssi));
    return;
  }

  if (!WiFi.isConnected())
    return;

  LogInfo("WiFi: roaming %ld dBm → %s at %ld dBm, channel %ld",
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
  wifiRoamHandoverPending = true;
}

static void startWifiRoamScan(unsigned long now, int32_t currentRssi, const char *reason)
{
  const uint8_t *currentBssid = WiFi.BSSID();
  if (currentBssid == nullptr)
    return;
  memcpy(wifiRoamSourceBssid, currentBssid, sizeof(wifiRoamSourceBssid));

  wifiRoamSourceRssi = currentRssi;
  lastWifiRoamAttempt = now;
  lowWifiRssiSince = 0;

  const int16_t result = WiFi.scanNetworks(true, false, false,
                                            wifiRoamScanTimePerChannel, 0,
                                            configuration.Wifi.SSID);
  if (result == WIFI_SCAN_RUNNING)
  {
    wifiRoamScanActive = true;
    LogInfo("WiFi: %s (%ld dBm), scanning for a better AP",
            reason, static_cast<long>(currentRssi));
    return;
  }

  wifiRoamConfirmPending = false;
  LogWarn("WiFi: roam scan could not start (result %d)", result);
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

  if (wifiRoamConfirmPending)
  {
    startWifiRoamScan(now, currentRssi, "confirming candidate");
    return;
  }

  if (wifiPostReconnectScanPending && now - wifiReconnectedAt >= wifiRoamPostReconnectDelay)
  {
    wifiPostReconnectScanPending = false;
    if (currentRssi < wifiRoamPostReconnectRssiThreshold)
    {
      startWifiRoamScan(now, currentRssi, "weak signal after reconnect");
      return;
    }
  }

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

  startWifiRoamScan(now, currentRssi, "weak signal");
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
    LogWarn("WiFi: no SSID configured, launching AP mode");
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
      // Our own handover already chose the best AP; boot used a full scan.
      wifiPostReconnectScanPending = wifiConnectedBefore && !wifiRoamHandoverPending;
      wifiReconnectedAt = millis();
      wifiConnectedBefore = true;
      wifiRoamHandoverPending = false;
      wifiRoamConfirmPending = false;
      LogInfo("WiFi: connected ✓ %s ch %ld, %ld dBm, IP %s",
              WiFi.BSSIDstr().c_str(), static_cast<long>(WiFi.channel()),
              static_cast<long>(WiFi.RSSI()), WiFi.localIP().toString().c_str());
      if (!MDNS.begin(configuration.Wifi.Hostname))
        LogWarn("WiFi: mDNS responder could not be started");
    }

    SyncTimeIfRequired();
    updateWifiRoaming();
  }
  else if (!SetupMode)
  {
    if (wifiWasConnected)
      LogWarn("WiFi: connection lost ✗, heating control stays active while reconnecting");
    wifiWasConnected = false;

    const unsigned long now = millis();
    if (lastWifiAttempt != 0 && now - lastWifiAttempt < wifiRetryInterval)
      return;
    lastWifiAttempt = now;
    // A failed handover falls back to this unpinned reconnect; treat its
    // result as an ordinary reconnect so the post-reconnect check runs.
    wifiRoamHandoverPending = false;

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

    LogDebug("WiFi: ⟳ connecting to %s as %s", configuration.Wifi.SSID, configuration.Wifi.Hostname);

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
    LogDebug("WiFi: %s, IP %s, mask %s, gateway %s, %ld dBm", WiFi.SSID().c_str(),
             WiFi.localIP().toString().c_str(), WiFi.subnetMask().toString().c_str(),
             WiFi.gatewayIP().toString().c_str(), static_cast<long>(WiFi.RSSI()));
}
