#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

struct WiFiConfig
{
    uint32_t timeout_ms;
    wifi_power_t tx_power;
    uint32_t reconnect_period_ms;
};

bool ensureWiFi(const WiFiConfig &cfg);

void setWifiSleep(bool enabled);

void setupOTA();
void sync_ntp_time();

int getRSSI();
