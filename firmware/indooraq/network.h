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

void sync_ntp_time();
