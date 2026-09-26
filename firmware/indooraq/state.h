#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_TSL2591.h>
#include <SparkFun_SCD4x_Arduino_Library.h>
#include "config.h"

////////////////////////////////////////////////////////////
// This header is the one place every module may pull shared
// runtime state from. Each global is *defined* exactly once,
// in state.cpp.
////////////////////////////////////////////////////////////

// ---------------------------------------------------------
// Hardware handles
// ---------------------------------------------------------
extern AsyncWebServer server;
extern WiFiClient wifiClient;
extern PubSubClient mqtt;

extern Adafruit_SHT4x sht4;
extern Adafruit_BMP280 bmp;
extern Adafruit_TSL2591 tsl;
extern SCD4x scd4;

// ---------------------------------------------------------
// Sensor presence / init status
// ---------------------------------------------------------
struct SensorStatus
{
  bool present = false;
  bool initialized = false;
};
extern SensorStatus shtStat, bmpStat, tslStat, scdStat;

extern bool i2cDevices[128];

// ---------------------------------------------------------
// Motion sensors
// ---------------------------------------------------------
struct am312Status
{
  bool present = true;
  bool motion = false;
  bool lastMotion = false;
  uint32_t lastMotionTs = 0;
};
extern am312Status am312;

// Duty-cycle aggregator: accumulates *time* spent with motion=true vs the
// total elapsed time, fed by read_motion_sensors() every loop() iteration
// (interrupt-driven since Phase 1 — see motion.cpp). This replaced a
// poll-fraction ("was motion true on N out of M samples taken every
// SAMPLE_INTERVAL") scheme, which could miss pulses shorter than the poll
// period entirely.
struct AM312Agg
{
  uint32_t motionMs = 0;
  uint32_t totalMs = 0;
  void addInterval(bool motion, uint32_t ms)
  {
    if (motion) motionMs += ms;
    totalMs += ms;
  }
  float fraction() const { return totalMs ? float(motionMs) / totalMs : 0.0f; }
  void reset() { motionMs = totalMs = 0; }
};
extern AM312Agg am312Agg;

struct LD1020Status
{
  bool present = false;
  bool motion = false;
  bool lastMotion = false;
  uint32_t lastMotionTs = 0;
};
extern LD1020Status ld1020;

// See AM312Agg above — same duty-cycle scheme for the LD1020 channel.
struct LD1020Agg
{
  uint32_t motionMs = 0;
  uint32_t totalMs = 0;
  void addInterval(bool motion, uint32_t ms)
  {
    if (motion) motionMs += ms;
    totalMs += ms;
  }
  float fraction() const { return totalMs ? float(motionMs) / totalMs : 0.0f; }
  void reset() { motionMs = totalMs = 0; }
};
extern LD1020Agg ld1020Agg;

// ---------------------------------------------------------
// Live + smoothed sensor readings
// ---------------------------------------------------------
extern float temp, hum, pres, lux;
extern float tempSmooth, humSmooth, luxSmooth;
extern float tempPrev, humPrev, co2Prev;
extern float tempGrad, humGrad, co2Grad;
extern float co2, co2Smooth;
extern float cpuTemp;

// ---------------------------------------------------------
// Aggregation helpers (5-minute publish window)
// ---------------------------------------------------------
struct AggMean
{
  float sum = 0;
  uint32_t count = 0;
  void add(float v) { sum += v; count++; }
  float mean() const { return count ? sum / count : NAN; }
  void reset() { sum = 0; count = 0; }
};

struct AggMinMax
{
  float sum = 0;
  float min = 1e6;
  float max = -1e6;
  uint32_t count = 0;
  void add(float v)
  {
    sum += v;
    count++;
    if (v < min) min = v;
    if (v > max) max = v;
  }
  float mean() const { return count ? sum / count : NAN; }
  void reset() { sum = 0; min = 1e6; max = -1e6; count = 0; }
};

extern AggMinMax tempAgg, humAgg;
extern AggMean presAgg, luxAgg, co2Agg;

// ---------------------------------------------------------
// Timing / scheduling state shared across loop() tasks
// ---------------------------------------------------------
extern time_t aggStartTs;
extern uint32_t lastSample;
extern uint32_t lastAgg;
extern uint32_t lastMqttTry;

// ---------------------------------------------------------
// WiFi state
// ---------------------------------------------------------
extern uint32_t wifiLastAttempt;
extern uint32_t wifiOfflineSince;
extern bool WIFI_SLEEP;

// ---------------------------------------------------------
// OTA state
// ---------------------------------------------------------
extern volatile bool otaInProgress;

// ---------------------------------------------------------
// Onboard-LED error signalling (see leds.cpp)
// ---------------------------------------------------------
// Catch-all "other error" flag any module can raise via
// report_other_error(true/false) (leds.h) for a condition that doesn't
// have its own dedicated onboard-LED code yet. No auto-timeout — whoever
// raises it is responsible for clearing it.
extern volatile bool otherErrorFlag;

// ---------------------------------------------------------
// Small shared utility
// ---------------------------------------------------------
inline float safe(float v) { return (isnan(v) || isinf(v)) ? 0.0f : v; }
