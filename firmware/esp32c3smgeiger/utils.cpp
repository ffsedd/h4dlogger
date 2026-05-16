#include <WiFi.h>
#include "utils.h"

int getRSSI()
{
    if (WiFi.status() != WL_CONNECTED)
        return 0;
    return WiFi.RSSI();
}

float getCpuTemp()
{
    return temperatureRead();
}
