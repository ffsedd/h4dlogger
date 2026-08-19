#include "network.h"
#include "wifi_secrets.h"
#include "build_config.h"

#include <ArduinoOTA.h>
#include <esp_wifi.h>
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

static bool reconnect_due(uint32_t period_ms)
{
    static uint32_t last_attempt = -UINT32_MAX;

    const uint32_t now = millis();

    if (now - last_attempt < period_ms)
        return false;

    last_attempt = now;

    return true;
}

static void prepare_wifi(const WiFiConfig &cfg)
{
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(static_cast<wifi_power_t>(cfg.tx_power));

    Serial.println("[WiFi] Interface prepared");
}

static int find_best_network()
{
    Serial.println("[WiFi] Scanning...");

    const int n = WiFi.scanNetworks();

    Serial.printf("[WiFi] Found %d networks\n", n);

    int best = -1;
    int best_rssi = -1000;

    for (int i = 0; i < n; ++i)
    {
        const String ssid = WiFi.SSID(i);
        const int rssi = WiFi.RSSI(i);

        Serial.printf(
            "[WiFi]   %-24s %4d dBm\n",
            ssid.c_str(),
            rssi);

        for (size_t j = 0; j < WIFI_NETWORK_COUNT; ++j)
        {
            if (ssid == WIFI_NETWORKS[j].ssid &&
                rssi > best_rssi)
            {
                best = static_cast<int>(j);
                best_rssi = rssi;
            }
        }
    }

    WiFi.scanDelete();

    if (best < 0)
    {
        Serial.println("[WiFi] No known network found");
        return -1;
    }

    Serial.printf(
        "[WiFi] Selected: %s (%d dBm)\n",
        WIFI_NETWORKS[best].ssid,
        best_rssi);

    return best;
}

static void connect_wifi(int network)
{
    Serial.printf(
        "[WiFi] Connecting to %s...\n",
        WIFI_NETWORKS[network].ssid);

    WiFi.begin(
        WIFI_NETWORKS[network].ssid,
        WIFI_NETWORKS[network].password);

    setWifiSleep(true); // WIFI SLEEP <=========================
}

bool ensureWiFi(const WiFiConfig &cfg)
{
    if (WiFi.status() == WL_CONNECTED)
        return true;

    if (!reconnect_due(cfg.reconnect_period_ms))
        return false;

    Serial.println("[WiFi] Connection required");

    prepare_wifi(cfg);

    const int network = find_best_network();

    if (network < 0)
        return false;

    connect_wifi(network);

    return false;
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