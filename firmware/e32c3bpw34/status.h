#pragma once

#include <Arduino.h>

struct Status
{
    float adc_raw;
    float adc_smooth;
    float signal;
    float adc_voltage;
    int wifi_rssi;
    float wifi_tx_power;
    float cpu_temp;
    bool wifi_connected;
    bool alarm_active;
    String wifi_ip;
    String wifi_ssid;
};

extern Status status;

void printStatus(const Status &s);