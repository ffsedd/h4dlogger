#include "status.h"
#include <time.h>

void printStatus(const Status &s)
{
    const time_t now = time(nullptr);
    struct tm tm_now;

    localtime_r(&now, &tm_now);

    Serial.printf(
        "%02d:%02d:%02d  ADC=%4.0f  EMA=%6.1f  Signal=%6.1f  V=%.3f  "
        "RSSI=%4d  TX=%4.1f  T=%4.1fC  WIFI=%s  IP=%s  ALARM=%s\n",
        tm_now.tm_hour,
        tm_now.tm_min,
        tm_now.tm_sec,
        s.adc_raw,
        s.adc_smooth,
        s.signal,
        s.adc_voltage,
        s.wifi_rssi,
        s.wifi_tx_power,
        s.cpu_temp,
        s.wifi_connected ? s.wifi_ssid.c_str() : "NO",
        s.wifi_ip.c_str(),
        s.alarm_active ? "YES" : "NO");
}