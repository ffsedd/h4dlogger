#include "status.h"
#include "config.h"
#include "state.h"

void print_sysinfo()
{
  Serial.println("========== SYSTEM ==========");

  Serial.printf("CPU: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Heap: %u (min %u)\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());

  Serial.printf("WiFi RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());

  Serial.println("\nSensors:");

  Serial.printf("SHT4x   : %s / %s\n",
                shtStat.present ? "present" : "missing",
                shtStat.initialized ? "OK" : "FAIL");

  Serial.printf("BMP280  : %s / %s\n",
                bmpStat.present ? "present" : "missing",
                bmpStat.initialized ? "OK" : "FAIL");

  Serial.printf("TSL2591 : %s / %s\n",
                tslStat.present ? "present" : "missing",
                tslStat.initialized ? "OK" : "FAIL");

  Serial.printf("SCD4x : %s / %s\n",
                scdStat.present ? "present" : "missing",
                scdStat.initialized ? "OK" : "FAIL");

  Serial.println("============================\n");
}

String jsonData()
{
  char buf[1600];

  snprintf(buf, sizeof(buf),
           "{"
           "\"device\":\"%s\","
           "\"ssid\":\"%s\","
           "\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,\"lux\":%.2f,"
           "\"temp_smooth\":%.2f,\"hum_smooth\":%.2f,"
           "\"temp_grad\":%.3f,\"hum_grad\":%.3f,"
           "\"co2\":%.0f,\"co2_smooth\":%.0f,\"co2_grad\":%.3f,"
           "\"ld1020_motion\":%d,\"am312_motion\":%d,"
           "\"wifi_rssi\":%d,\"wifi_ch\":%d,"
           "\"cpu_temp\":%.2f,"
           "\"cpu_freq\":%d,"
           "\"heap\":%u,\"heap_min\":%u,"
           "\"uptime\":%lu,"
           "\"ip\":\"%s\","
           "\"mqtt\":%s"
           "}",
           DEVICE_ID,
           WiFi.SSID().c_str(),
           safe(temp), safe(hum), safe(pres), safe(lux),
           safe(tempSmooth), safe(humSmooth),
           safe(tempGrad), safe(humGrad),
           safe(co2), safe(co2Smooth), safe(co2Grad),
           ld1020.motion ? 1 : 0,
           am312.motion ? 1 : 0,
           WiFi.RSSI(),
           WiFi.channel(),
           safe(cpuTemp),
           ESP.getCpuFreqMHz(),
           ESP.getFreeHeap(),
           ESP.getMinFreeHeap(),
           millis() / 1000,
           WiFi.localIP().toString().c_str(),
           mqtt.connected() ? "true" : "false");

  return String(buf);
}
