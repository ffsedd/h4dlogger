#pragma once

#include <Arduino.h>
#include <WiFi.h>

void connectWiFi(uint32_t timeout_ms = 1000,
                  wifi_power_t tx_power = WIFI_POWER_8_5dBm);

void reconnectWiFi();
void setupOTA();
void sync_ntp_time();