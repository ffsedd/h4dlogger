#pragma once
#include <Arduino.h>
#include <WiFi.h>

struct WifiHotspots
{
  const char *ssid;
  const char *password;
};
// The actual wifihotspots[] array is defined in wifi_secrets.h, which is
// included only by network.cpp (it needs the array's real size for sizeof()).

int find_best_wifi();
const char *wifi_status_str(wl_status_t s);
bool connect_best_wifi(unsigned long timeout = 8000);
bool wait_for_wifi(uint32_t timeout_ms = 15000);

void wifi_watchdog();
void WiFiEvent(WiFiEvent_t event);

// Starts the background FreeRTOS task that owns WiFi.scanNetworks() /
// WiFi.begin() for every reconnect after boot (Phase 2). Call once from
// setup(), after the initial synchronous connect_best_wifi()/wait_for_wifi()
// sequence has run. wifi_watchdog() then just raises a request flag instead
// of calling connect_best_wifi() inline.
void start_wifi_task();

void sync_ntp_time();
