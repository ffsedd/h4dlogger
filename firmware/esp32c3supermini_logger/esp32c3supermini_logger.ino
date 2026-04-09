#pragma once
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <Arduino.h> // for ledc* functions
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_TSL2591.h>
#include <SparkFun_SCD4x_Arduino_Library.h>
#include <LittleFS.h>

////////////////////////////////////////////////////////////
// CONFIG
////////////////////////////////////////////////////////////

#define MQTT_HOST "10.11.12.1"
#define MQTT_PORT 1883

//  ======================= DEVICE NAME ===============================
#define DEVICE_ID "bedroom"

// ======================
// LED pins
// ======================
constexpr gpio_num_t PIN_LED_R = GPIO_NUM_4;
constexpr gpio_num_t PIN_LED_Y = GPIO_NUM_3;
constexpr gpio_num_t PIN_LED_G = GPIO_NUM_1;

constexpr uint8_t CH_R = 0;
constexpr uint8_t CH_G = 1;
constexpr uint8_t CH_Y = 2;

// ======================
// Motion sensors
// ======================
constexpr gpio_num_t PIN_LD1020 = GPIO_NUM_0;
bool LD1020_PRESENT = false;
constexpr gpio_num_t PIN_AM312 = GPIO_NUM_10;
bool AM312_PRESENT = true;
// ======================
// I2C
// ======================
constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_7;
constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_6;

#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#define SAMPLE_INTERVAL 500     // 0.5 s
#define SAMPLE_INTERVAL_SCD40 5000 // 5 s
#define AGG_INTERVAL 300000     // 5 min
#define FILTER_N 5

#define LOG_VALUES 0

#define LOG_MOTION_EVENTS 1
#define LOG_MQTT_EVENTS 1
#define LOG_LED_EVENTS 1

void startWeb(); // in file web.ino

struct WifiHotspots
{ // in file "wifi_secrets.h"
  const char *ssid;
  const char *password;
};
#include "wifi_secrets.h"

////////////////////////////////////////////////////////////
// WIFI STATE
////////////////////////////////////////////////////////////

uint32_t wifiLastAttempt = 0;
constexpr uint32_t WIFI_RETRY_MS = 30000;   // 30 s
constexpr uint32_t WIFI_REBOOT_MS = 3600000; // 1 h
uint32_t wifiOfflineSince = 0;


////////////////////////////////////////////////////////////
// UTILS: RUNNING MEAN
////////////////////////////////////////////////////////////

template <int N>
struct RunningMean
{
  float buf[N] = {0};
  int idx = 0;
  int count = 0;
  float sum = 0;

  float add(float v)
  {
    if (count < N)
    {
      buf[idx] = v;
      sum += v;
      count++;
    }
    else
    {
      sum -= buf[idx];
      buf[idx] = v;
      sum += v;
    }
    idx = (idx + 1) % N;
    return sum / count;
  }
};

////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////

RunningMean<FILTER_N> tempF, humF, luxF;

float temp = NAN, hum = NAN, pres = NAN, lux = NAN;
float tempSmooth = NAN, humSmooth = NAN, luxSmooth = NAN;
float tempPrev = NAN, humPrev = NAN, co2Prev = NAN;
float tempGrad = 0, humGrad = 0, co2Grad = 0;
float co2 = NAN;
float co2Smooth = NAN;

time_t aggStartTs = 0;
uint32_t lastSample = 0;
uint32_t lastAgg = 0;
uint32_t lastMqttTry = 0;

////////////////////////////////////////////////////////////
// MOTION SENSORS
////////////////////////////////////////////////////////////
constexpr uint32_t MOTION_HOLD_MS = 30*60*1000; // time to hold LED on after motion detected (ms)

struct am312Status
{
  bool present = true;
  bool motion = false;
  bool lastMotion = false;
  uint32_t lastMotionTs = 0;
} am312;

struct AM312Agg
{
  uint32_t motionCount = 0;
  uint32_t totalCount = 0;
  void add(bool motion)
  {
    if (motion)
      motionCount++;
    totalCount++;
  }
  float fraction() const { return totalCount ? float(motionCount) / totalCount : 0.0f; }
  void reset() { motionCount = totalCount = 0; }
} am312Agg;

struct LD1020Status
{
  bool present = false;
  bool motion = false;
  bool lastMotion = false;
  uint32_t lastMotionTs = 0;
} ld1020;
struct LD1020Agg
{
  uint32_t motionCount = 0;
  uint32_t totalCount = 0;
  void add(bool motion)
  {
    if (motion)
      motionCount++;
    totalCount++;
  }
  float fraction() const { return totalCount ? float(motionCount) / totalCount : 0.0f; }
  void reset() { motionCount = totalCount = 0; }
} ld1020Agg;

////////////////////////////////////////////////////////////
// HW / NETWORK
////////////////////////////////////////////////////////////

AsyncWebServer server(80);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Adafruit_SHT4x sht4;
Adafruit_BMP280 bmp;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
SCD4x scd4;

////////////////////////////////////////////////////////////
// LED
////////////////////////////////////////////////////////////
constexpr uint8_t PWM_RES = 8;      // PWM resolution (bits)
constexpr uint32_t PWM_FREQ = 1000; // PWM frequency (Hz)

enum LedState
{
  LED_LOW,
  LED_MID,
  LED_HIGH
};
LedState co2LedState = LED_LOW;

////////////////////////////////////////////////////////////
// SENSOR STATUS
////////////////////////////////////////////////////////////

struct SensorStatus
{
  bool present = false;
  bool initialized = false;
};
SensorStatus shtStat, bmpStat, tslStat, scdStat;

bool i2cDevices[128] = {false};

////////////////////////////////////////////////////////////
// AGGREGATION
////////////////////////////////////////////////////////////

struct AggMean
{
  float sum = 0;
  uint32_t count = 0;
  void add(float v)
  {
    sum += v;
    count++;
  }
  float mean() const { return count ? sum / count : NAN; }
  void reset()
  {
    sum = 0;
    count = 0;
  }
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
    if (v < min)
      min = v;
    if (v > max)
      max = v;
  }
  float mean() const { return count ? sum / count : NAN; }
  void reset()
  {
    sum = 0;
    min = 1e6;
    max = -1e6;
    count = 0;
  }
};

AggMinMax tempAgg, humAgg;
AggMean presAgg, luxAgg, co2Agg;

////////////////////////////////////////////////////////////
// FILTERS
////////////////////////////////////////////////////////////

struct EMA
{
  float tau, value;
  EMA(float tau_sec) : tau(tau_sec), value(NAN) {}
  float update(float v, float dt)
  {
    float a = dt / (tau + dt);
    if (isnan(value))
      value = v;
    else
      value += a * (v - value);
    return value;
  }
};

EMA tempLPF(10.0f), humLPF(10.0f), luxLPF(5.0f), co2LPF(5.0f);
EMA tempGradLPF(120.0f), humGradLPF(120.0f), co2GradLPF(180.0f);

float tempHist[3] = {NAN, NAN, NAN}, humHist[3] = {NAN, NAN, NAN}, co2Hist[3] = {NAN, NAN, NAN};

inline void push3(float *h, float v)
{
  h[0] = h[1];
  h[1] = h[2];
  h[2] = v;
}

////////////////////////////////////////////////////////////
// FUNCTION DECLARATIONS
////////////////////////////////////////////////////////////

void scan_i2c_devices();
bool is_known_i2c_addr(uint8_t addr);
void init_i2c_sensors();
void setup_LD1020();
void setup_AM312();
void read_fast_sensors();
void read_SCD40();
void read_sensors();
void print_sysinfo();
void sync_ntp_time();
void updateLED();
void mqttSendCSV(unsigned long ts, const char *sensor, const char *metric, float value, bool retained = false);
void publish_Agg_values();

////////////////////////////////////////////////////////////
// IMPLEMENTATION
////////////////////////////////////////////////////////////

bool is_known_i2c_addr(uint8_t addr) { return addr == 0x44 || addr == 0x76 || addr == 0x77 || addr == 0x29 || addr == 0x62; }

void scan_i2c_devices()
{
  Serial.println("\n========== I2C SCAN ==========");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++)
  {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0)
    {
      i2cDevices[addr] = true;
      Serial.printf("[I2C] 0x%02X  %s\n", addr, is_known_i2c_addr(addr) ? "KNOWN" : "UNKNOWN ⚠");
      found++;
    }
    else if (err == 4)
    {
      Serial.printf("[I2C] 0x%02X ERROR\n", addr);
    }
  }
  Serial.printf("[I2C] Total: %u\n", found);
  Serial.println("===============================\n");
}

void init_i2c_sensors()
{
  Serial.println("[BOOT] Detecting sensors...");
  shtStat.present = i2cDevices[0x44];
  if (shtStat.present)
  {
    shtStat.initialized = sht4.begin(&Wire);
    Serial.printf("[SHT4x] init... %s\n", shtStat.initialized ? "OK" : "FAIL");
  }
  uint8_t bmpAddr = i2cDevices[0x76] ? 0x76 : (i2cDevices[0x77] ? 0x77 : 0);
  bmpStat.present = bmpAddr != 0;
  if (bmpStat.present)
  {
    bmpStat.initialized = bmp.begin(bmpAddr);
    Serial.printf("[BMP280] init... %s\n", bmpStat.initialized ? "OK" : "FAIL");
  }
  tslStat.present = i2cDevices[0x29];
  if (tslStat.present)
  {
    tslStat.initialized = tsl.begin();
    if (tslStat.initialized)
    {
      tsl.setGain(TSL2591_GAIN_MED);
      tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    }
    Serial.printf("[TSL2591] init... %s\n", tslStat.initialized ? "OK" : "FAIL");
  }
  scdStat.present = i2cDevices[0x62];
  if (scdStat.present)
  {
    if (scd4.begin())
    {
      scdStat.initialized = true;
      scd4.startPeriodicMeasurement();
      Serial.println("[SCD4x] init... OK");
    }
    else
      Serial.println("[SCD4x] init... FAIL");
  }
}



void setupLED()
{
  ledcAttach(PIN_LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LED_Y, PWM_FREQ, PWM_RES);

  ledcWrite(PIN_LED_R, 0);
  ledcWrite(PIN_LED_G, 0);
  ledcWrite(PIN_LED_Y, 0);
}



void setup_LD1020()
{
  if (LD1020_PRESENT)
  {
    pinMode(PIN_LD1020, INPUT);
    ld1020.present = true;
    ld1020.motion = digitalRead(PIN_LD1020);
    ld1020.lastMotion = ld1020.motion;
    Serial.printf("[LD1020] initialized on GPIO%d\n", PIN_LD1020);
  }
  else
  {
    ld1020.present = false;
    ld1020.motion = false;
    Serial.println("[LD1020] not present");
  }
}
void setup_AM312()
{
  if (AM312_PRESENT)
  {
    pinMode(PIN_AM312, INPUT);
    am312.present = true;
    am312.motion = digitalRead(PIN_AM312);
    am312.lastMotion = am312.motion;
    Serial.printf("[AM312] initialized on GPIO%d\n", PIN_AM312);
  }
  else
  {
    am312.present = false;
    am312.motion = false;
    Serial.println("[AM312] not present");
  }
}

void read_fast_sensors()
{
  const float dt = SAMPLE_INTERVAL / 1000.0f;
  if (ld1020.present)
  {
    ld1020.motion = digitalRead(PIN_LD1020);
    ld1020Agg.add(ld1020.motion);
    if (ld1020.motion)
      ld1020.lastMotionTs = millis();
    if (ld1020.motion && !ld1020.lastMotion)
      Serial.println("[LD1020] motion detected! =====================");
    ld1020.lastMotion = ld1020.motion;
  }
  if (am312.present)
  {
    am312.motion = digitalRead(PIN_AM312);
    am312Agg.add(am312.motion);
    if (am312.motion)
      am312.lastMotionTs = millis();
    if (am312.motion && !am312.lastMotion)
      Serial.println("[AM312] motion detected! --------------------");
    am312.lastMotion = am312.motion;
  }
  if (shtStat.initialized)
  {
    sensors_event_t h, t;
    if (sht4.getEvent(&h, &t))
    {
      temp = t.temperature;
      hum = h.relative_humidity;
      tempSmooth = tempLPF.update(temp, dt);
      humSmooth = humLPF.update(hum, dt);
      push3(tempHist, tempSmooth);
      push3(humHist, humSmooth);
      if (!isnan(tempHist[0]))
        tempGrad = tempGradLPF.update((tempHist[2] - tempHist[0]) / (2.0f * dt), dt);
      if (!isnan(humHist[0]))
        humGrad = humGradLPF.update((humHist[2] - humHist[0]) / (2.0f * dt), dt);
      tempAgg.add(tempSmooth);
      humAgg.add(humSmooth);
    }
  }
  if (bmpStat.initialized)
  {
    pres = bmp.readPressure() / 100.0f;
    presAgg.add(pres);
  }
  if (tslStat.initialized)
  {
    sensors_event_t light;
    tsl.getEvent(&light);
    if (!isnan(light.light) && light.light > 0 && light.light < 200000)
    {
      lux = luxLPF.update(light.light, dt);
      luxAgg.add(lux);
    }
  }

  if (LOG_VALUES)
    Serial.printf("[FastSensors] %.2f C | %.2f %% | %.1f hPa | %.1f lx\n", temp, hum, pres, lux);
}

void read_SCD40()
{
  static uint32_t lastSCDSample = 0, lastTs = 0;
  uint32_t now = millis();

  if (!scdStat.initialized)
    return;

  if (now - lastSCDSample < SAMPLE_INTERVAL_SCD40)
    return;

  lastSCDSample = now;

  // Attempt measurement
  if (!scd4.readMeasurement())
  {
    co2 = NAN;
    co2Smooth = NAN;
    co2Grad = NAN;
    lastTs = now;
    return;
  }

  float newCO2 = scd4.getCO2();
  if (newCO2 <= 0 || newCO2 > 10000)
  {
    co2 = NAN;
    co2Smooth = NAN;
    co2Grad = NAN;
    lastTs = now;
    return;
  }

  // Valid reading
  float dt = (lastTs == 0) ? SAMPLE_INTERVAL_SCD40 / 1000.0f : (now - lastTs) / 1000.0f;
  lastTs = now;

  co2 = newCO2;
  co2Smooth = co2LPF.update(co2, dt);
  co2Agg.add(co2Smooth);
  push3(co2Hist, co2Smooth);

  if (!isnan(co2Hist[0]))
    co2Grad = co2GradLPF.update((co2Hist[2] - co2Hist[0]) / (2.0f * dt), dt);

  if (LOG_VALUES)
    Serial.printf("[CO2] %.1f | smooth: %.1f | grad: %.3f ppm/s\n", co2, co2Smooth, co2Grad);
}

void read_sensors()
{
  read_fast_sensors();
  read_SCD40();
}

////////////////////////////////////////////////////////////
// SYSTEM INFO
////////////////////////////////////////////////////////////

void print_sysinfo()
{

  Serial.println("========== SYSTEM ==========");

  Serial.printf("CPU: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Heap: %u (min %u)\n",
                ESP.getFreeHeap(),
                ESP.getMinFreeHeap());

  Serial.printf("WiFi RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());

  Serial.println("\nSensors:");

  Serial.printf("SHT4x   : %s / %s\n",
                shtStat.present ? "present" : "missing",
                shtStat.initialized ? "OK" : "FAIL");

  Serial.printf("BMP280  : %s / %s\n",
                bmpStat.present ? "present" : "missing",
                bmpStat.initialized ? "OK" : "FAIL");

  Serial.printf("TSL2591 : %s / %s\n",
                tslStat.present ? "present" : "missing",
                tslStat.initialized ? "OK" : "FAIL");

  Serial.printf("SCD4x : %s / %s\n",
                scdStat.present ? "present" : "missing",
                scdStat.initialized ? "OK" : "FAIL");

  Serial.println("============================\n");
}


////////////////////////////////////////////////////////////
// WIFI
////////////////////////////////////////////////////////////
int find_best_wifi()
{
  WiFi.mode(WIFI_STA);

  int bestRSSI = -1000;
  int bestIndex = -1;

  int n = WiFi.scanNetworks();

  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    for (int j = 0; j < sizeof(wifihotspots) / sizeof(wifihotspots[0]); j++)
    {
      if (ssid == wifihotspots[j].ssid && rssi > bestRSSI)
      {
        bestRSSI = rssi;
        bestIndex = j;
      }
    }
  }

  WiFi.scanDelete(); // critical
  delay(200);

  return bestIndex;
}

const char *wifi_status_str(wl_status_t s)
{
  switch (s)
  {
  case WL_IDLE_STATUS:
    return "IDLE";
  case WL_NO_SSID_AVAIL:
    return "NO_SSID";
  case WL_SCAN_COMPLETED:
    return "SCAN_DONE";
  case WL_CONNECTED:
    return "CONNECTED";
  case WL_CONNECT_FAILED:
    return "FAILED";
  case WL_CONNECTION_LOST:
    return "LOST";
  case WL_DISCONNECTED:
    return "DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

bool connect_best_wifi(unsigned long timeout = 8000)
{
  int idx = find_best_wifi();

  if (idx == -1)
  {
    Serial.println("No known network found");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  Serial.printf("Connecting to %s\n", wifihotspots[idx].ssid);

  WiFi.begin(
      wifihotspots[idx].ssid,
      wifihotspots[idx].password);

  return true;
}
////////////////////////////////////////////////////////////
// NTP
////////////////////////////////////////////////////////////

void sync_ntp_time()
{
    configTzTime(TZ_INFO, NTP_SERVER);

    Serial.println("[NTP] Waiting for time sync...");
    time_t now = 0;
    int retry = 0;
    const int max_retry = 30; // ~15s
    while ((now = time(nullptr)) < 24*3600 && retry < max_retry)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }
    if (retry >= max_retry)
        Serial.println("\n[NTP] Time sync failed!");
    else
        Serial.println("\n[NTP] Time synced!");
}
////////////////////////////////////////////////////////////
// LED OUTPUT
////////////////////////////////////////////////////////////

enum class BlinkMode : uint8_t
{
  NONE,
  GREEN_IDLE,   // 200 ms / 2 s
  SLOW_RED,     // 1 Hz
  FAST_RED      // 3 Hz
};

static inline uint8_t lerp8(float t)
{
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  return (uint8_t)(t * 255.0f);
}

void updateLED()
{
  const uint32_t now = millis();

  ////////////////////////////////////////////////////////////
  // MOTION GATE
  ////////////////////////////////////////////////////////////

  uint32_t lastMotionTs =
      max(ld1020.lastMotionTs, am312.lastMotionTs);

  if (lastMotionTs == 0 ||
      (now - lastMotionTs) > MOTION_HOLD_MS)
  {
    ledcWrite(PIN_LED_G,0);
    ledcWrite(PIN_LED_Y,0);
    ledcWrite(PIN_LED_R,0);
    return;
  }

  ////////////////////////////////////////////////////////////
  // CO2 VALIDITY
  ////////////////////////////////////////////////////////////

  if (isnan(co2Smooth))
  {
    ledcWrite(PIN_LED_G,0);
    ledcWrite(PIN_LED_Y,0);
    ledcWrite(PIN_LED_R,0);
    return;
  }

  ////////////////////////////////////////////////////////////
  // DETERMINE STATE
  ////////////////////////////////////////////////////////////

  float co2 = co2Smooth;

  uint8_t g=0,y=0,r=0;
  BlinkMode blinkMode = BlinkMode::NONE;

  if (co2 < 600)
  {
    g = 255;
    blinkMode = BlinkMode::GREEN_IDLE;
  }
  else if (co2 < 800)
  {
    g = 255;
  }
  else if (co2 < 1000)
  {
    float t = (co2 - 800.f)/200.f;
    g = lerp8(1.f - t);
    y = lerp8(t);
  }
  else if (co2 < 1200)
  {
    y = 255;
  }
  else if (co2 < 1400)
  {
    float t = (co2 - 1200.f)/200.f;
    y = lerp8(1.f - t);
    r = lerp8(t);
  }
  else if (co2 < 1600)
  {
    r = 255;
  }
  else if (co2 < 1800)
  {
    r = 255;
    blinkMode = BlinkMode::SLOW_RED;
  }
  else
  {
    r = 255;
    blinkMode = BlinkMode::FAST_RED;
  }

  ////////////////////////////////////////////////////////////
  // BLINK ENGINE (non-blocking)
  ////////////////////////////////////////////////////////////

  static uint32_t phaseStart = 0;
  static bool blinkOn = true;

  uint32_t period = 0;
  uint32_t onTime = 0;

  switch (blinkMode)
  {
    case BlinkMode::GREEN_IDLE:
      period = 2000;
      onTime = 200;
      break;

    case BlinkMode::SLOW_RED:
      period = 2000;
      onTime = 1000;
      break;

    case BlinkMode::FAST_RED:
      period = 500;
      onTime = 250;
      break;

    default:
      break;
  }

  if (blinkMode != BlinkMode::NONE)
  {
    if (now - phaseStart >= period)
      phaseStart = now;

    blinkOn = (now - phaseStart) < onTime;
  }
  else
  {
    blinkOn = true;
  }

  ////////////////////////////////////////////////////////////
  // APPLY OUTPUT
  ////////////////////////////////////////////////////////////

  if (!blinkOn)
    g=y=r=0;

  ledcWrite(PIN_LED_G,g);
  ledcWrite(PIN_LED_Y,y);
  ledcWrite(PIN_LED_R,r);

  ////////////////////////////////////////////////////////////
  // RATE-LIMITED LOGGING
  ////////////////////////////////////////////////////////////

  if (LOG_LED_EVENTS)
  {
    static uint32_t lastLog=0;
    if (now-lastLog>5000)
    {
      lastLog=now;
      Serial.printf(
        "[LED] CO2=%.1f G=%u Y=%u R=%u mode=%u\n",
        co2,g,y,r,(uint8_t)blinkMode);
    }
  }
}
////////////////////////////////////////////////////////////
// MQTT OUTPUT
////////////////////////////////////////////////////////////

// --------------------------------------------------------
// MQTT CSV Sender
// topic   = device/sensor/metric
// payload = timestamp,value
// --------------------------------------------------------
void mqttSendCSV(
    unsigned long ts,
    const char *sensor,
    const char *metric,
    float value,
    bool retained)
{
  if (!mqtt.connected())
    return;

  char topic[96];
  char payload[64];

  // topic: kitchen/sht40/temp
  snprintf(
      topic,
      sizeof(topic),
      "%s/%s/%s",
      DEVICE_ID,
      sensor,
      metric);

  // payload: 1774671828,22.341
  snprintf(
      payload,
      sizeof(payload),
      "%lu,%.3f",
      ts,
      value);

  mqtt.publish(
      topic,
      (uint8_t *)payload,
      strlen(payload),
      retained);
}

////////////////////////////////////////////////////////////
// PUBLISH AGGREGATED DATA
////////////////////////////////////////////////////////////
void publish_Agg_values()
{
  if (!mqtt.connected())
    return;

  //--------------------------------------------------------
  // Initialize aggregation window start
  //--------------------------------------------------------
  if (aggStartTs == 0)
    aggStartTs = time(nullptr);

  //--------------------------------------------------------
  // Correct CENTER timestamp of aggregation interval
  //--------------------------------------------------------
  const time_t ts_center =
      aggStartTs + (AGG_INTERVAL / 2000); // ms → s /2

  //--------------------------------------------------------
  // ---------- Temperature ----------
  //--------------------------------------------------------
  if (tempAgg.count)
  {
    mqttSendCSV(ts_center, "sht40", "temp_mean", tempAgg.mean());
    mqttSendCSV(ts_center, "sht40", "temp_min", tempAgg.min);
    mqttSendCSV(ts_center, "sht40", "temp_max", tempAgg.max);
  }

  //--------------------------------------------------------
  // ---------- Humidity ----------
  //--------------------------------------------------------
  if (humAgg.count)
  {
    mqttSendCSV(ts_center, "sht40", "rh_mean", humAgg.mean());
    mqttSendCSV(ts_center, "sht40", "rh_min", humAgg.min);
    mqttSendCSV(ts_center, "sht40", "rh_max", humAgg.max);
  }

  //--------------------------------------------------------
  // ---------- Pressure ----------
  //--------------------------------------------------------
  if (presAgg.count)
    mqttSendCSV(ts_center, "bmp280", "pressure_mean", presAgg.mean());

  //--------------------------------------------------------
  // ---------- Light ----------
  //--------------------------------------------------------
  if (luxAgg.count)
    mqttSendCSV(ts_center, "tsl2591", "lux_mean", luxAgg.mean());

  //--------------------------------------------------------
  // ---------- Gradients (VALIDITY GUARDED)
  // publish only if enough samples exist
  //--------------------------------------------------------
  const uint16_t MIN_GRAD_SAMPLES = 3;

  if (tempAgg.count > MIN_GRAD_SAMPLES)
    mqttSendCSV(ts_center, "sht40", "temp_grad", tempGrad);

  if (humAgg.count > MIN_GRAD_SAMPLES)
    mqttSendCSV(ts_center, "sht40", "rh_grad", humGrad);

  if (co2Agg.count > MIN_GRAD_SAMPLES)
    mqttSendCSV(ts_center, "scd40", "co2_grad", co2Grad);

  //--------------------------------------------------------
  // ---------- System ----------
  //--------------------------------------------------------
  mqttSendCSV(ts_center, "system", "wifi_rssi", WiFi.RSSI());
  mqttSendCSV(ts_center, "system", "heap", ESP.getFreeHeap());
  mqttSendCSV(ts_center, "system", "heap_min", ESP.getMinFreeHeap());
  mqttSendCSV(ts_center, "system", "uptime", millis() / 1000.0f);

  //--------------------------------------------------------
  // ---------- CO2 ----------
  //--------------------------------------------------------
  mqttSendCSV(ts_center, "scd40", "co2_smooth", co2Smooth);

  //--------------------------------------------------------
  // ---------- LD1020 motion ----------
  //--------------------------------------------------------
  if (ld1020Agg.totalCount > 0)
    mqttSendCSV(ts_center, "ld1020", "motion_fraction",
                ld1020Agg.fraction());

  // ---------- AM312 motion ----------
  if (am312Agg.totalCount > 0)
    mqttSendCSV(ts_center, "am312", "motion_fraction",
                am312Agg.fraction());

  //--------------------------------------------------------
  // RESET AGGREGATORS
  //--------------------------------------------------------
  tempAgg.reset();
  humAgg.reset();
  presAgg.reset();
  luxAgg.reset();
  ld1020Agg.reset();
  am312Agg.reset();
  co2Agg.reset(); // IMPORTANT for gradient validity

  //--------------------------------------------------------
  // START NEW AGGREGATION WINDOW
  //--------------------------------------------------------
  aggStartTs = time(nullptr);
  lastAgg = millis();

  //--------------------------------------------------------
  // ---- SERIAL PRINT ALL ----
  //--------------------------------------------------------
  if (LOG_MQTT_EVENTS)
  {
    Serial.printf("[MQTT] sht40 temp=%.2f temp_smooth=%.2f temp_grad=%.3f\n",
                  temp, tempSmooth, tempGrad);

    Serial.printf("[MQTT] sht40 hum=%.2f hum_smooth=%.2f hum_grad=%.3f\n",
                  hum, humSmooth, humGrad);

    Serial.printf("[MQTT] scd40 co2=%.0f co2_smooth=%.0f co2_grad=%.3f\n",
                  co2, co2Smooth, co2Grad);

    Serial.printf("[MQTT] bmp280 pressure=%.2f\n", pres);
    Serial.printf("[MQTT] tsl2591 lux=%.2f\n", lux);
    Serial.printf("[MQTT] ld1020 motion=%d\n", ld1020.motion ? 1 : 0);
  }
}

////////////////////////////////////////////////////////////
// MQTT CONNECTION
////////////////////////////////////////////////////////////
void connect_MQTT()
{
  if (mqtt.connected())
    return;

  if (millis() - lastMqttTry < 5000)
    return;

  lastMqttTry = millis();

  Serial.print("MQTT connecting... ");

  //------------------------------------------------------
  // Build LWT topic
  //------------------------------------------------------
  char topicStatus[64];
  snprintf(
      topicStatus,
      sizeof(topicStatus),
      "%s/system/status",
      DEVICE_ID);

  //------------------------------------------------------
  // Last Will payload (offline)
  //------------------------------------------------------
  char willPayload[64];
  snprintf(
      willPayload,
      sizeof(willPayload),
      "%lu,0",
      (unsigned long)time(nullptr));

  //------------------------------------------------------
  // Connect
  //------------------------------------------------------
  if (mqtt.connect(
          DEVICE_ID,
          topicStatus, // LWT topic
          1,
          true, // retain
          willPayload))
  {
    Serial.println("OK");

    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(10);

    // publish ONLINE retained
    mqttSendCSV(
        time(nullptr),
        "system",
        "status",
        1,
        true);
  }
  else
  {
    Serial.print("FAIL rc=");
    Serial.println(mqtt.state());
  }
}
////////////////////////////////////////////////////////////
// WEB JSON - REALTIME STATUS ENDPOINT
////////////////////////////////////////////////////////////

String jsonData()
{
  auto safe = [](float v)
  { return isnan(v) || isinf(v) ? 0.0f : v; };

  char buf[1500]; // slightly larger for SSID

  snprintf(buf, sizeof(buf),
           "{\"device\":\"%s\","
           "\"ssid\":\"%s\","
           "\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,\"lux\":%.2f,"
           "\"temp_smooth\":%.2f,\"hum_smooth\":%.2f,"
           "\"temp_grad\":%.3f,\"hum_grad\":%.3f,"
           "\"co2\":%.0f,\"co2_smooth\":%.0f,\"co2_grad\":%.3f,"
           "\"ld1020_motion\":%d,\"am312_motion\":%d,"
           "\"wifi_rssi\":%d,\"wifi_ch\":%d,"
           "\"heap\":%u,\"heap_min\":%u,"
           "\"uptime\":%lu,\"ip\":\"%s\",\"mqtt\":%s}",
           DEVICE_ID,
           WiFi.SSID().c_str(),
           safe(temp), safe(hum), safe(pres), safe(lux),
           safe(tempSmooth), safe(humSmooth),
           safe(tempGrad), safe(humGrad),
           safe(co2), safe(co2Smooth), safe(co2Grad),
           ld1020.motion ? 1 : 0,
           am312.motion ? 1 : 0,
           WiFi.RSSI(), WiFi.channel(),
           ESP.getFreeHeap(), ESP.getMinFreeHeap(),
           millis() / 1000,
           WiFi.localIP().toString().c_str(),
           mqtt.connected() ? "true" : "false");

  return String(buf);
}
////////////////////////////////////////////////////////////
// OTA
////////////////////////////////////////////////////////////
volatile bool otaInProgress = false;

void setupOTA()
{
    ArduinoOTA.setHostname(DEVICE_ID);

    ArduinoOTA
        .onStart([]() {
            Serial.println("[OTA] Start");
        })
        .onEnd([]() {
            Serial.println("\n[OTA] End");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("[OTA] %u%%\r", progress * 100 / total);
        })
        .onError([](ota_error_t error) {
            Serial.printf("[OTA] Error[%u]\n", error);
        });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}



////////////////////////////////////////////////////////
// WIFI WATCHDOG
////////////////////////////////////////////////////////
void wifi_watchdog()
{
    wl_status_t st = WiFi.status();


    if (st == WL_CONNECTED)
    {
        wifiOfflineSince = 0;
        return;
    }

    if (wifiOfflineSince == 0)
        wifiOfflineSince = millis();


    if (millis() - wifiLastAttempt > WIFI_RETRY_MS)
    {
        wifiLastAttempt = millis();

        Serial.println("[WiFi] reconnect attempt");

        connect_best_wifi(8000);   // short timeout
    }


    if (millis() - wifiOfflineSince > WIFI_REBOOT_MS)
    {
        Serial.println("[WiFi] offline too long -> reboot");
        ESP.restart();
    }
}




void WiFiEvent(WiFiEvent_t event)
{
  switch(event)
  {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("[WiFi] IP %s\n",
            WiFi.localIP().toString().c_str());
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("[WiFi] disconnected");
        break;

    default:
        break;
  }
}


bool wait_for_wifi(uint32_t timeout_ms = 15000)
{
  uint32_t start = millis();

  while (!WiFi.isConnected())
  {
    delay(100);

    if (millis() - start > timeout_ms)
    {
      Serial.println("[WiFi] timeout");
      return false;
    }
  }
  return true;
}
////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////
void setup()
{
  // ---------- SERIAL ----------
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println("\n===== BOOT =====");
  Serial.printf("Reset reason: %d\n", esp_reset_reason());

  // ---------- I2C / HARDWARE FIRST ----------
  Wire.setTimeout(50);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);

  scan_i2c_devices();
  init_i2c_sensors();

  setup_LD1020();
  setup_AM312();
  setupLED();

  // ---------- WIFI STACK ----------
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.disconnect(true, true);

  delay(200);

  connect_best_wifi();

  // ---------- WAIT FOR CONNECTION ----------
  wait_for_wifi();     // IMPORTANT

  // ---------- STATUS ----------
  if (WiFi.isConnected())
  {
    Serial.printf("[WiFi] IP=%s RSSI=%d\n",
      WiFi.localIP().toString().c_str(),
      WiFi.RSSI());
  }

  print_sysinfo();

  // ---------- TIME ----------
  sync_ntp_time();

  // ---------- NETWORK SERVICES ----------
  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  startWeb();      // needs IP
  setupOTA();      // safest last
}

////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////
void loop()
{
  ArduinoOTA.handle();

  static uint32_t lastWifiRetry = 0;
  uint32_t now = millis();

  // ---------- WIFI WATCHDOG ----------
  wifi_watchdog();

  // ---------- MQTT ----------
  connect_MQTT();
  mqtt.loop();

  // ---------- SENSORS ----------
  if (!otaInProgress && (now - lastSample >= SAMPLE_INTERVAL))
  {
    lastSample = now;
    read_sensors();
  }

  if (!otaInProgress && (now - lastAgg >= AGG_INTERVAL))
  {
    lastAgg = now;
    publish_Agg_values();
  }
  
  updateLED();

  delay(1);
}
