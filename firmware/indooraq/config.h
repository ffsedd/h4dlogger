#pragma once
#include <Arduino.h>

////////////////////////////////////////////////////////////
// CONFIG
////////////////////////////////////////////////////////////

#define MQTT_HOST "10.11.12.1"
#define MQTT_PORT 1883

// ======================= LOAD CONFIG ===============================
#include "build_config.h" // contains DEVICE_ID

// ======================
// LED pins
// ======================
constexpr gpio_num_t PIN_LED_R = GPIO_NUM_4;
constexpr gpio_num_t PIN_LED_Y = GPIO_NUM_3;
constexpr gpio_num_t PIN_LED_G = GPIO_NUM_1;

constexpr uint8_t CH_R = 0;
constexpr uint8_t CH_G = 1;
constexpr uint8_t CH_Y = 2;

constexpr uint8_t PWM_RES = 8;       // PWM resolution (bits)
constexpr uint32_t PWM_FREQ = 200;   // PWM frequency (Hz)

// ======================
// Onboard status LED (single color, active LOW)
// ======================
// GPIO8 — free in this project (I2C is remapped to 6/7 below, not the
// default 8/9), used to blink out a 4-bit error code. See leds.cpp.
constexpr gpio_num_t PIN_ONBOARD_LED = GPIO_NUM_8;

// ======================
// Motion sensors
// ======================
constexpr gpio_num_t PIN_LD1020 = GPIO_NUM_0;
constexpr bool LD1020_PRESENT = false;
constexpr gpio_num_t PIN_AM312 = GPIO_NUM_10;
constexpr bool AM312_PRESENT = true;

constexpr uint32_t MOTION_HOLD_MS = 30 * 60 * 1000; // hold LED on after motion (ms)

// ======================
// I2C
// ======================
constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_7;
constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_6;

#define CPU_FREQ_MHZ 80 // 40=too low, wifi fails, 80=power saving, 240 max

#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#define SAMPLE_INTERVAL 2000          // 2 s
#define SAMPLE_INTERVAL_SCD40 10000   // 10 s
#define AGG_INTERVAL 300000           // 5 min
#define FILTER_N 5

#define LOG_VALUES 0
#define LOG_MOTION_EVENTS 0
#define LOG_MQTT_EVENTS 0
#define LOG_LED_EVENTS 0

// ======================
// WiFi watchdog timing
// ======================
constexpr uint32_t WIFI_RETRY_MS = 60000;    // 60 s
constexpr uint32_t WIFI_REBOOT_MS = 3600000; // 1 h



// CO2 LED thresholds [ppm]
constexpr float CO2_EXCELLENT = 600.0f;
constexpr float CO2_GOOD = 800.0f;
constexpr float CO2_ELEVATED = 1000.0f;
constexpr float CO2_HIGH = 1200.0f;
constexpr float CO2_VENTILATE = 1500.0f;
constexpr float CO2_SLOW_BLINK = 1800.0f;
constexpr float CO2_FAST_BLINK = 2000.0f;