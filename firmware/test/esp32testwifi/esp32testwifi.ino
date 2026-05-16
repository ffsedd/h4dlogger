#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_system.h>

#ifdef CONFIG_IDF_TARGET_ESP32C3
  #include "esp_private/esp_clk.h"
#endif

struct WifiHotspots
{
    const char *ssid;
    const char *password;
};

#include "wifi_secrets.h"


// ------------------------------------------------------------
// STATE
// ------------------------------------------------------------
static uint32_t attempt = 0;
static uint32_t last_print = 0;


// ------------------------------------------------------------
// WIFI STATUS STRING
// ------------------------------------------------------------
const char *wifi_status_str(wl_status_t s)
{
    switch (s)
    {
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "NO_SSID";
        case WL_SCAN_COMPLETED: return "SCAN_DONE";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "FAIL";
        case WL_CONNECTION_LOST: return "LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}




// ------------------------------------------------------------
// OPTIONAL TEMP (C3 MAY BE INVALID -> SAFE GUARD)
// ------------------------------------------------------------
float read_chip_temp()
{
#ifdef CONFIG_IDF_TARGET_ESP32C3
    // Often unsupported / unreliable depending on core version
    return NAN;
#else
    return temperatureRead();
#endif
}


// ------------------------------------------------------------
// EXTRA DIAG (RF + POWER + CPU LOAD PROXY)
// ------------------------------------------------------------
void print_diag()
{
    static uint32_t last_cycle = 0;
    uint32_t now = micros();
    uint32_t jitter = now - last_cycle;
    last_cycle = now;

    float t = temperatureRead();  

    Serial.printf(
        "[DIAG] heap=%u RSSI=%d status=%s uptime=%lu ms attempt=%lu jitter=%lu us",
        ESP.getFreeHeap(),
        WiFi.RSSI(),
        wifi_status_str(WiFi.status()),
        millis(),
        attempt,
        jitter
    );

    if (!isnan(t))
        Serial.printf(" temp=%.2fC", t);

    Serial.println();
}


// ------------------------------------------------------------
// WIFI EVENTS
// ------------------------------------------------------------
void wifi_event(arduino_event_id_t event)
{
    Serial.printf("[EV %d] ", event);

    switch (event)
    {
        case ARDUINO_EVENT_WIFI_READY:
            Serial.println("READY");
            break;

        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("STA_START");
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("L2_CONNECTED");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("IP=");
            Serial.println(WiFi.localIP());
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.print("DISCONNECTED");

            break;

        default:
            Serial.println("OTHER");
            break;
    }
}


// ------------------------------------------------------------
// CONNECT LOGIC
// ------------------------------------------------------------
bool connect_all_wifi(uint32_t timeout_ms = 8000)
{
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    WiFi.disconnect(true, true);
    delay(1000);

    const int N = sizeof(wifihotspots) / sizeof(wifihotspots[0]);

    Serial.printf("Configured SSIDs: %d\n", N);

    for (int i = 0; i < N; i++)
    {
        attempt++;
        Serial.printf("\nTRY SSID: %s\n", wifihotspots[i].ssid);

        WiFi.begin(wifihotspots[i].ssid,
                   wifihotspots[i].password);

        uint32_t t0 = millis();

        while (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - last_print > 500)
            {
                last_print = millis();
                print_diag();
            }

            delay(100);

            if (millis() - t0 > timeout_ms)
            {
                Serial.println("TIMEOUT");
                break;
            }
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("\nCONNECTED OK");
            print_diag();
            return true;
        }

        WiFi.disconnect(true, true);
        delay(300);
    }

    Serial.println("\nALL SSIDs FAILED");
    return false;
}


// ------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== WIFI DIAGNOSTIC MODE (EXTENDED) ===");

    esp_log_level_set("wifi", ESP_LOG_VERBOSE);

    WiFi.onEvent(wifi_event);

    connect_all_wifi();
}


// ------------------------------------------------------------
void loop()
{
    static uint32_t last = 0;

    if (millis() - last > 2000)
    {
        last = millis();
        print_diag();
    }
    delay(100);
}
