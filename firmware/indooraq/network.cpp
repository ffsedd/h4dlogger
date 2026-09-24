#include "network.h"
#include "config.h"
#include "state.h"
#include "leds.h" // led_ntp_success_sequence()

#include "wifi_secrets.h" // defines wifihotspots[] against the WifiHotspots
                           // struct declared in network.h — included here
                           // only, so sizeof(wifihotspots) works below.

////////////////////////////////////////////////////////////
// WIFI
////////////////////////////////////////////////////////////

int find_best_wifi()
{
  WiFi.mode(WIFI_STA);

  int bestRSSI = -1000;
  int bestIndex = -1;

  int n = WiFi.scanNetworks();

  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    for (int j = 0; j < (int)(sizeof(wifihotspots) / sizeof(wifihotspots[0])); j++)
    {
      if (ssid == wifihotspots[j].ssid && rssi > bestRSSI)
      {
        bestRSSI = rssi;
        bestIndex = j;
      }
    }
  }

  WiFi.scanDelete(); // critical
  delay(200);

  return bestIndex;
}

const char *wifi_status_str(wl_status_t s)
{
  switch (s)
  {
  case WL_IDLE_STATUS:
    return "IDLE";
  case WL_NO_SSID_AVAIL:
    return "NO_SSID";
  case WL_SCAN_COMPLETED:
    return "SCAN_DONE";
  case WL_CONNECTED:
    return "CONNECTED";
  case WL_CONNECT_FAILED:
    return "FAILED";
  case WL_CONNECTION_LOST:
    return "LOST";
  case WL_DISCONNECTED:
    return "DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

bool connect_best_wifi(unsigned long timeout)
{
  int idx = find_best_wifi();

  if (idx == -1)
  {
    Serial.println("No known network found");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_SLEEP);

  Serial.printf("Connecting to %s\n", wifihotspots[idx].ssid);

  WiFi.begin(wifihotspots[idx].ssid, wifihotspots[idx].password);

  return true;
}

bool wait_for_wifi(uint32_t timeout_ms)
{
  uint32_t start = millis();

  while (!WiFi.isConnected())
  {
    delay(100);

    if (millis() - start > timeout_ms)
    {
      Serial.println("[WiFi] timeout");
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// WIFI WATCHDOG
////////////////////////////////////////////////////////////

void wifi_watchdog()
{
  wl_status_t st = WiFi.status();

  if (st == WL_CONNECTED)
  {
    wifiOfflineSince = 0;
    return;
  }

  if (wifiOfflineSince == 0)
    wifiOfflineSince = millis();

  if (millis() - wifiLastAttempt > WIFI_RETRY_MS)
  {
    wifiLastAttempt = millis();
    Serial.println("[WiFi] reconnect attempt");
    connect_best_wifi(8000); // short timeout
  }

  if (millis() - wifiOfflineSince > WIFI_REBOOT_MS)
  {
    Serial.println("[WiFi] offline too long -> reboot");
    ESP.restart();
  }
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial.printf("[WiFi] IP %s\n", WiFi.localIP().toString().c_str());
    break;

  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial.println("[WiFi] disconnected");
    break;

  default:
    break;
  }
}

////////////////////////////////////////////////////////////
// NTP
////////////////////////////////////////////////////////////

void sync_ntp_time()
{
  configTzTime(TZ_INFO, NTP_SERVER);

  Serial.println("[NTP] Waiting for time sync...");
  time_t now = 0;
  int retry = 0;
  const int max_retry = 30; // ~15s
  while ((now = time(nullptr)) < 24 * 3600 && retry < max_retry)
  {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (retry >= max_retry)
  {
    Serial.println("\n[NTP] Time sync failed!");
  }
  else
  {
    Serial.println("\n[NTP] Time synced!");
    led_ntp_success_sequence();
  }
}
