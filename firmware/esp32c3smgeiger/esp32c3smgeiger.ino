#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <string.h>

#include "wifi_secrets.h"
#include "utils.h"
#include "lcd_ui.h"

#include <ArduinoOTA.h>

#include "build_config.h"
#include "geiger_core.h"

/* =====================================================
   External INTERFACES
===================================================== */

void startWeb();
void handleWebLoop();

/* =====================================================
   CONFIG
===================================================== */

constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_7;
constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_6;
static constexpr int GEIGER_PIN = 10;

#define CPU_FREQ_MHZ 80

static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t CPS_WINDOW = 600;

/* =====================================================
   SENSOR STATUS
===================================================== */

struct SensorStatus
{
    bool present = false;
    bool initialized = false;
};

SensorStatus shtStat, bmpStat, tslStat, scdStat;

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
        uint32_t idx =
            bufFull ? (bufIndex + i) % CPS_WINDOW : i;

        sum += cpsBuffer[idx];
    }

    return sum / n;
}
/* =====================================================
   STATS UPDATE
===================================================== */

void updateStats()
{
    uint32_t now = millis();
    if (now - lastSampleMs < SAMPLE_INTERVAL_MS)
        return;

    lastSampleMs = now;

    noInterrupts();
    uint32_t pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    cps = pulses;
    cpsMean = avg();

    cpsBuffer[bufIndex] = cps;
    avgBuffer[bufIndex] = cpsMean;

    bufIndex++;

    if (bufIndex >= CPS_WINDOW)
    {
        bufIndex = 0;
        bufFull = true;
    }

    Serial.printf(
        "CPS=%.2f AVG=%.2f RSSI=%d TX_POWER=%.1f CPU=%.1fC\n",
        cps,
        cpsMean,
        getRSSI(),
        (int)WiFi.getTxPower() / 4,
        temperatureRead());
}

/* =====================================================
   SETUP
===================================================== */

void setup()
{
    Serial.begin(115200);
    delay(500);

    setCpuFrequencyMhz(CPU_FREQ_MHZ);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);

    lcdInit();
    scan_i2c_devices();

    connectWiFi();
    sync_ntp_time();
    setupOTA();

    startWeb();

    pinMode(GEIGER_PIN, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(GEIGER_PIN),
        geigerISR,
        FALLING);

    Serial.println("System ready");
}

/* =====================================================
   LOOP
===================================================== */

void loop()
{
    ArduinoOTA.handle();
    handleWebLoop();

    updateStats();
    lcdUpdate(cps, cpsMean);
}
