#include "status.h"
#include <Arduino.h>

void printStatus(const Status &s)
{
    Serial.print("CPS=");
    Serial.print(s.geiger_cps);

    Serial.print(" EMA=");
    Serial.print(s.geiger_cps_smooth);

    Serial.print(" TOTAL=");
    Serial.print((uint32_t)s.geiger_total_count);

    Serial.print(" RSSI=");
    Serial.print(s.wifi_rssi);

    Serial.print(" TX=");
    Serial.print(s.wifi_tx_power);

    Serial.print(" T=");
    Serial.print(s.cpu_temp);

    Serial.print("C WIFI=");
    Serial.print(s.wifi_connected);

    Serial.print(" ALARM=");
    Serial.println(s.alarm_active);
}