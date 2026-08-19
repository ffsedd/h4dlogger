#include <WiFi.h>
#include <ArduinoOTA.h>
#include <chrono>

#include "wifi_secrets.h"
#include "network.h"
#include "build_config.h"
#include "threshold_alarm.hpp"
#include "status.h"
#include "web.h"
#include "buzzer.h"

using steady_clock = std::chrono::steady_clock;

/* =========================
   CONFIG
========================= */

constexpr int ANALOG_PIN = 1; // OPA380 output
constexpr int LED_PIN = 4;

#define CPU_FREQ_MHZ 80

static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;

/* EMA smoothing */
static constexpr float EMA_ALPHA = 0.4f;

/* Alarm threshold - tune after measuring the X-ray signal */
static constexpr float ALARM_THRESHOLD = 900.0f;
static constexpr float ALARM_HYSTERESIS = 100.0f;

/* =========================
   WIFI
========================= */

WiFiConfig wifi_cfg = {
    .timeout_ms = 15000,
    .tx_power = WIFI_POWER_8_5dBm,
    .reconnect_period_ms = 30000};

Status status;

bool PRINT_STATUS_TO_SERIAL = true;

/* =========================
   GLOBAL STATE
========================= */

static soundAlarm::ThresholdAlarm soundAlarmEngine(
    {ALARM_THRESHOLD,
     ALARM_HYSTERESIS,
     std::chrono::seconds(2)});

volatile bool alarmActive = false;

static float adc_ema = 0.0f;
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

static bool adc_initialized = false;

void updateStatus()
{
    const float adc_raw = analogRead(ANALOG_PIN);

    if (!adc_initialized)
    {
        adc_ema = adc_raw;
        adc_initialized = true;
    }
    else
    {
        adc_ema = ema_update(adc_ema, adc_raw);
    }

    status.adc_raw = adc_raw;
    status.adc_smooth = adc_ema;
    status.adc_voltage = adc_ema * 3.3f / 4095.0f;
    status.signal = 4095.0f - adc_ema;

    status.wifi_rssi = getRSSI();
    status.wifi_tx_power = WiFi.getTxPower() / 4.0f;
    status.cpu_temp = temperatureRead();

    status.wifi_connected = WiFi.isConnected();
    status.wifi_ssid = WiFi.SSID();
    status.wifi_ip = WiFi.localIP().toString();
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

    Serial.println("\n=== BPW34 X-ray Detector ===");

    analogReadResolution(12);
    analogSetPinAttenuation(ANALOG_PIN, ADC_11db);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);

    ensureWiFi(wifi_cfg);

    startWeb();
    setupOTA();

    soundAlarmEngine.set_callback([](soundAlarm::State s)
                                  { alarmActive = (s == soundAlarm::State::Alarm); });

    Serial.println("System ready");
}

/* =========================
   LOOP
========================= */
void loop()
{
    ArduinoOTA.handle();
    handleWebLoop();

    updateBuzzer(status.signal);

    const uint32_t now = millis();

    if (now - lastSampleMs >= SAMPLE_INTERVAL_MS)
    {
        ensureWiFi(wifi_cfg);

        lastSampleMs += SAMPLE_INTERVAL_MS;

        updateStatus();

        soundAlarmEngine.update(
            status.signal,
            steady_clock::now());

        if (PRINT_STATUS_TO_SERIAL)
            printStatus(status);
    }

    digitalWrite(LED_PIN, alarmActive ? HIGH : LOW);

    delay(10);
}