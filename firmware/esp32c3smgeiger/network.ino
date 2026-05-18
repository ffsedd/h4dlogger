#include "network.h"

#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

/* =====================================================
   WIFI
===================================================== */

void connectWiFi(
    uint32_t timeout_ms,
    wifi_power_t tx_power)
{
    if (WiFi.isConnected())
    {
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(tx_power);

    Serial.print("WiFi connecting");

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    const uint32_t t0 = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - t0 > timeout_ms)
        {
            Serial.println("\nWiFi connection failed");
            return;
        }

        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
}


void reconnectWiFi()
{
    static uint32_t last_attempt_ms = 0;

    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    uint32_t now = millis();

    if (now - last_attempt_ms < 5000)
    {
        return;
    }

    last_attempt_ms = now;

    Serial.println("WiFi reconnecting...");
    connectWiFi();
}


/* =====================================================
   OTA
===================================================== */

void setupOTA()
{
    ArduinoOTA.setHostname(DEVICE_ID);
    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

/* =====================================================
   NTP
===================================================== */

void sync_ntp_time()
{
    configTzTime(TZ_INFO, NTP_SERVER);

    Serial.print("[NTP] Sync");

    time_t now;
    int retry = 0;

    do
    {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    } while (now < 100000 && retry++ < 30);

    Serial.println("\n[NTP] Done");
}
