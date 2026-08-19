#pragma once

#include <WiFi.h>

struct WiFiConfig
{
    uint32_t timeout_ms;
    wifi_power_t tx_power;
    uint32_t reconnect_period_ms;
};

void setWifiSleep(bool enabled);
bool ensureWiFi(const WiFiConfig &cfg);
void setupOTA();
void sync_ntp_time();
int getRSSI();