#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <string.h>

#include "wifi_secrets.h"
#include "network.h"
#include "utils.h"
#include "lcd_ui.h"
// #include "oled_ui.h"
#include <ArduinoOTA.h>

#include "build_config.h"

#include "threshold_alarm.hpp"
#include <iostream>
#include <chrono>

using steady_clock = std::chrono::steady_clock;

static soundAlarm::ThresholdAlarm soundAlarmEngine(
    {10, 1, std::chrono::seconds(2)});

volatile bool alarmActive = false;
constexpr int BUZZER_CHANNEL = 0;
constexpr int BUZZER_PIN = 3;
constexpr int PWM_FREQ = 2000;
constexpr int PWM_RES = 8;

/* =====================================================
   External INTERFACES
===================================================== */

void startWeb();
void handleWebLoop();

/* =====================================================
   CONFIG
===================================================== */
constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_6;
constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_7;

static constexpr int GEIGER_PIN = 1;

#define CPU_FREQ_MHZ 80

static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t CPS_WINDOW = 30;

/* =====================================================
   GEIGER STATE
===================================================== */

volatile uint32_t pulseCount = 0;

float cpsBuffer[CPS_WINDOW];
float avgBuffer[CPS_WINDOW];

uint32_t bufIndex = 0;
bool bufFull = false;

float cps = 0;
float cpsMean = 0;

uint32_t lastSampleMs = 0;

uint32_t getBufferSize()
{
    return bufFull ? CPS_WINDOW : bufIndex;
}

/* =====================================================
   ISR
===================================================== */

void IRAM_ATTR geigerISR()
{
    pulseCount++;
}

/* =====================================================
   MOVING AVERAGE
===================================================== */

float avg()
{
    uint32_t n = getBufferSize();
    if (!n)
        return 0;

    float sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t idx = (bufIndex + CPS_WINDOW - n + i) % CPS_WINDOW;
        sum += cpsBuffer[idx];
    }

    return sum / n;
}
/* =====================================================
   STATS UPDATE
===================================================== */

void readSensors()
{

    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    cps = pulses;

    cpsBuffer[bufIndex] = cps;
    cpsMean = avg();
    avgBuffer[bufIndex] = cpsMean;

    bufIndex++;

    if (bufIndex >= CPS_WINDOW)
    {
        bufIndex = 0;
        bufFull = true;
    }
}

/* =====================================================
   SETUP
===================================================== */

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=== Starting ESP32-C3 Geiger Counter ===");
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Flash Size: %u bytes\n", ESP.getFlashChipSize());
    Serial.printf("Flash Speed: %u Hz\n", ESP.getFlashChipSpeed());
    Serial.printf("Flash Mode: %s\n", ESP.getFlashChipMode() == FM_QIO ? "QIO" : "DIO");
    Serial.printf("Unique ID: %s\n", WiFi.macAddress().c_str());

    Serial.println("Connecting to WiFi...");

    connectWiFi();

    ledcAttach(BUZZER_PIN, PWM_FREQ, PWM_RES);
    ledcAttachChannel(BUZZER_CHANNEL, PWM_FREQ, PWM_RES, 0);
    ledcWriteTone(BUZZER_CHANNEL, 0);

    pinMode(GEIGER_PIN, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(GEIGER_PIN),
        geigerISR,
        FALLING);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);

    lcdInit();
    scan_i2c_devices();

    startWeb();

    setupOTA();

    sync_ntp_time();

    soundAlarmEngine.set_callback([](soundAlarm::State s)
                                  {
    if (s == soundAlarm::State::Alarm)
    {
        Serial.println("ALARM!");
        alarmActive = true;
    }
    else
    {
        alarmActive = false;
    } });

    setCpuFrequencyMhz(CPU_FREQ_MHZ);

    Serial.println("System ready");
}

/* =====================================================
   LOOP
===================================================== */

void loop()
{
    reconnectWiFi();
    ArduinoOTA.handle();

    handleWebLoop();

    uint32_t now = millis();
    if (now - lastSampleMs >= SAMPLE_INTERVAL_MS)
    {
        lastSampleMs += SAMPLE_INTERVAL_MS;

        readSensors();

        soundAlarmEngine.update(cpsMean, steady_clock::now());
        Serial.printf(
            "CPS=%.2f AVG=%.2f RSSI=%d TX_POWER=%.1f CPU=%.1fC\n",
            cps,
            cpsMean,
            getRSSI(),
            WiFi.getTxPower() / 4.0f,
            temperatureRead());

        lcdUpdate(cps, cpsMean);
    }

    if (alarmActive)
    {
        static uint32_t lastBeep = 0;
        static bool beepOn = false;

        uint32_t now = millis();

        // pattern: 2 beeps every ~600ms (Geiger-style alarm)
        if (!beepOn && now - lastBeep > 400)
        {
            ledcWriteTone(BUZZER_CHANNEL, 2800); // frequency in Hz (sharp audible tone)
            beepOn = true;
            lastBeep = now;
        }
        else if (beepOn && now - lastBeep > 120)
        {
            ledcWriteTone(BUZZER_CHANNEL, 0); // silence
            beepOn = false;
            lastBeep = now;
        }
    }
    else
    {
        ledcWriteTone(BUZZER_CHANNEL, 0); // always off
    }
}
