#include "network.h"
#include "wifi_secrets.h"
#include "build_config.h"
#include <esp_wifi.h>
#include <ArduinoOTA.h>
#include <time.h>

#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

/* =====================================================
   WIFI
===================================================== */

void setWifiSleep(bool enabled)
{
    esp_wifi_set_ps(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
}

bool ensureWiFi(const WiFiConfig &cfg)
{
    static uint32_t last_attempt = 0;

    if (WiFi.status() == WL_CONNECTED)
        return true;

    uint32_t now = millis();
    if (now - last_attempt < cfg.reconnect_period_ms)
        return false;

    last_attempt = now;

    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    WiFi.setTxPower(cfg.tx_power);

    // IMPORTANT: always disable sleep during connect
    esp_wifi_set_ps(WIFI_PS_NONE);

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - t0 > cfg.timeout_ms)
            return false;

        delay(50);
    }

    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());

    return true;
}

/* =====================================================
   OTA
===================================================== */

void setupOTA()
{
    ArduinoOTA.setHostname(DEVICE_ID);
    ArduinoOTA.begin();

    Serial.print("[OTA] Ready. Hostname: ");
    Serial.println(DEVICE_ID);
}

/* =====================================================
   NTP
===================================================== */

void sync_ntp_time()
{
    configTzTime(TZ_INFO, NTP_SERVER);

    Serial.print("[NTP] Sync");

    time_t t = time(nullptr);

    for (int i = 0; i < 30 && t < 100000; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        t = time(nullptr);
    }

    Serial.println("\n[NTP] Done");
}

/* =====================================================
   STATUS
===================================================== */

int getRSSI()
{
    if (WiFi.status() != WL_CONNECTED)
        return 0;
    return WiFi.RSSI();
}