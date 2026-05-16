#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

/* =====================================================
   WIFI
===================================================== */

void connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // hack for connection failures?
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("WiFi");

    while (!WiFi.isConnected())
    {
        delay(300);
        Serial.print(".");
    }

    Serial.print("\nWifi Connected, IP: ");
    Serial.println(WiFi.localIP());
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
