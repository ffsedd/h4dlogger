#include "network.h"
#include "config.h"
#include "state.h"
#include "leds.h" // led_start_ntp_success_sequence()

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
// PHASE 2 — BACKGROUND WIFI TASK
//
// WiFi.scanNetworks() (inside connect_best_wifi() -> find_best_wifi())
// blocks for several seconds. Calling it inline from wifi_watchdog() used
// to stall MQTT keepalive, OTA polling, sensor sampling and the LED
// update for that whole time, every WIFI_RETRY_MS while disconnected.
//
// It now runs on its own low-priority FreeRTOS task. loop() (via
// wifi_watchdog()) just raises a request flag and polls for the result;
// the ESP32-C3 is single-core, so this isn't parallel execution, but the
// scheduler still preempts this task's blocking scan to keep servicing
// the loop() task, so the stall no longer propagates to MQTT/OTA/LED.
//
// All WiFi.scanNetworks()/WiFi.begin() calls after boot go exclusively
// through this task -- no other code calls them concurrently, since the
// WiFi driver isn't designed to be driven from two contexts at once.
////////////////////////////////////////////////////////////

static TaskHandle_t wifiTaskHandle = nullptr;
static volatile bool wifiReconnectRequested = false;
static volatile bool wifiResultReady = false;
static volatile bool wifiConnectResult = false;

static void wifiTask(void *)
{
  for (;;)
  {
    if (wifiReconnectRequested)
    {
      wifiReconnectRequested = false;
      bool ok = connect_best_wifi(8000); // short timeout, same as before
      wifiConnectResult = ok;
      wifiResultReady = true;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void start_wifi_task()
{
  if (wifiTaskHandle)
    return; // already running

  xTaskCreate(wifiTask, "wifiTask", 4096, nullptr, 1, &wifiTaskHandle);
  Serial.println("[WiFi] background reconnect task started");
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

  if (millis() - wifiLastAttempt > WIFI_RETRY_MS && !wifiReconnectRequested)
  {
    wifiLastAttempt = millis();
    Serial.println("[WiFi] reconnect requested (background task)");
    wifiReconnectRequested = true;
  }

  if (wifiResultReady)
  {
    wifiResultReady = false;
    Serial.printf("[WiFi] background reconnect %s\n",
                   wifiConnectResult ? "issued WiFi.begin() to best known AP" : "found no known AP in range");
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
    led_start_ntp_success_sequence();
  }
}
