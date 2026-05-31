#pragma once
#include <stdint.h>

struct Status
{
    float geiger_cps;
    float geiger_cps_smooth;
    uint64_t geiger_total_count;

    int wifi_rssi;
    float wifi_tx_power;
    float cpu_temp;

    bool wifi_connected;
    bool alarm_active;
};

void printStatus(const Status &s);