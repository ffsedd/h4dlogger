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

struct AM312Agg
{
  uint32_t motionCount = 0;
  uint32_t totalCount = 0;
  void add(bool motion)
  {
    if (motion) motionCount++;
    totalCount++;
  }
  float fraction() const { return totalCount ? float(motionCount) / totalCount : 0.0f; }
  void reset() { motionCount = totalCount = 0; }
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

struct LD1020Agg
{
  uint32_t motionCount = 0;
  uint32_t totalCount = 0;
  void add(bool motion)
  {
    if (motion) motionCount++;
    totalCount++;
  }
  float fraction() const { return totalCount ? float(motionCount) / totalCount : 0.0f; }
  void reset() { motionCount = totalCount = 0; }
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
// Small shared utility
// ---------------------------------------------------------
inline float safe(float v) { return (isnan(v) || isinf(v)) ? 0.0f : v; }
