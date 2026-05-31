#include <WiFi.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <chrono>

#include "wifi_secrets.h"
#include "network.h"
#include "oled_ui.h"
#include "build_config.h"
#include "threshold_alarm.hpp"
#include "geiger_interface.h"
#include "status.h"

static constexpr int GEIGER_PIN = 10; /* =================================== GEIGER COUNTER PIN (GPIO10) =============================== */

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "geiger_pcnt.h"
static GeigerPCNT geiger(GEIGER_PIN);
#else
#include "geiger_isr.h"
static GeigerISR geiger(GEIGER_PIN);
#endif

using steady_clock = std::chrono::steady_clock;

/* =========================
   CONFIG
========================= */

constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_6; /* =================================== I2C SCL PIN (GPIO6) =============================== */
constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_7; /* =================================== I2C SDA PIN (GPIO7) =============================== */

#define CPU_FREQ_MHZ 80

static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;

/* EMA smoothing (tune this) */
static constexpr float EMA_ALPHA = 0.15f;

WiFiConfig wifi_cfg = {
    .timeout_ms = 15000,
    .tx_power = WIFI_POWER_8_5dBm,
    .reconnect_period_ms = 10000};

static Status status;

/* =========================
   GLOBAL STATE
========================= */

static soundAlarm::ThresholdAlarm soundAlarmEngine(
    {10, 1, std::chrono::seconds(2)});

volatile bool alarmActive = false;

constexpr int BUZZER_PIN = 3; /* =================================== BUZZER PIN (GPIO3) =============================== */

constexpr int BUZZER_CHANNEL = 0;
constexpr int PWM_FREQ = 2000;
constexpr int PWM_RES = 8;

static float cps_ema = 0.0f;
static uint32_t lastSampleMs = 0;

/* =========================
   EXTERNAL
========================= */

void startWeb();
void handleWebLoop();

/* =========================
   SENSOR UPDATE
========================= */

static inline float ema_update(float prev, float x)
{
    return prev + EMA_ALPHA * (x - prev);
}

void updateStatus()
{
    const float cps_raw = geiger.readAndReset();
    Serial.print("Raw CPS: ");
    Serial.println(cps_raw);

    cps_ema = ema_update(cps_ema, cps_raw);

    status.geiger_cps = cps_raw;
    status.geiger_cps_smooth = cps_ema;
    status.geiger_total_count = geiger.totalCount();

    status.wifi_rssi = getRSSI();
    status.wifi_tx_power = WiFi.getTxPower() / 4.0f;
    status.cpu_temp = temperatureRead();

    status.wifi_connected = WiFi.isConnected();
    status.alarm_active = alarmActive;
}

/* =========================
   SETUP
========================= */

void setup()
{
    Serial.begin(115200);

    setCpuFrequencyMhz(CPU_FREQ_MHZ);
    delay(300);

    Serial.println("\n=== Geiger Counter ===");

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    ensureWiFi(wifi_cfg);

    ledcAttach(BUZZER_PIN, PWM_FREQ, PWM_RES);
    ledcAttachChannel(BUZZER_CHANNEL, PWM_FREQ, PWM_RES, 0);
    ledcWriteTone(BUZZER_CHANNEL, 0);

    if (!geiger.begin())
    {
        Serial.println("Geiger init failed");
        while (true)
            delay(1000);
    }

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    lcdInit();
    startWeb();
    setupOTA();
    sync_ntp_time();

    soundAlarmEngine.set_callback([](soundAlarm::State s)
                                  { alarmActive = (s == soundAlarm::State::Alarm); });

    Serial.println("System ready");
}

/* =========================
   LOOP
========================= */

void loop()
{
    ensureWiFi(wifi_cfg);
    ArduinoOTA.handle();
    handleWebLoop();

    const uint32_t now = millis();

    if (now - lastSampleMs >= SAMPLE_INTERVAL_MS)
    {
        lastSampleMs += SAMPLE_INTERVAL_MS;

        updateStatus();
        soundAlarmEngine.update(status.geiger_cps_smooth, steady_clock::now());

        printStatus(status);

        lcdUpdate(status.geiger_cps);
    }

    if (alarmActive)
    {
        static uint32_t lastBeep = 0;
        static bool beepOn = false;

        const uint32_t t = millis();

        if (!beepOn && t - lastBeep > 400)
        {
            ledcWriteTone(BUZZER_CHANNEL, 2800);
            beepOn = true;
            lastBeep = t;
        }
        else if (beepOn && t - lastBeep > 120)
        {
            ledcWriteTone(BUZZER_CHANNEL, 0);
            beepOn = false;
            lastBeep = t;
        }
    }
    else
    {
        ledcWriteTone(BUZZER_CHANNEL, 0);
    }
    delay(10);
}