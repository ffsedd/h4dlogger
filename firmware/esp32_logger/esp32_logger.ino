#include <WiFi.h> //
#include <Wire.h>

#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "wifi_secrets.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_TSL2591.h>
#include <esp_task_wdt.h>
#include <SparkFun_SCD4x_Arduino_Library.h>

////////////////////////////////////////////////////////////
// CONFIG
////////////////////////////////////////////////////////////

#define MQTT_HOST "10.11.12.1"
#define MQTT_PORT 1883
#define MQTT_DEVICE_ID "kitchen"

#define I2C_SDA 21
#define I2C_SCL 22

#define WDT_TIMEOUT 120 // watchdog timeout in seconds

#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// ================= CONFIG =================

#define SAMPLE_INTERVAL 500        // 0.5 s sampling
#define SAMPLE_INTERVAL_SCD40 5000 // 5 s sampling for SCD40 (slow sensor)
#define AGG_INTERVAL 300000        // 5 min aggregation
#define FILTER_N 5                 // 5 s running mean
#define SERIAL_PRINT_VALUES 1

// ================= RUNNING MEAN =================
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

// ================= FILTERS =================

RunningMean<FILTER_N> tempF;
RunningMean<FILTER_N> humF;
RunningMean<FILTER_N> luxF;

// ================= PRIMARY VALUES =================

float temp = NAN;
float hum = NAN;
float pres = NAN;
float lux = NAN;

// ================= FILTERED VALUES =================

float tempSmooth = NAN;
float humSmooth = NAN;
float luxSmooth = NAN;

// ================= DERIVATIVES =================

float tempPrev = NAN;
float humPrev = NAN;
float co2Prev = NAN;

float tempGrad = 0;
float humGrad = 0;
float co2Grad = 0;

// ================= CO2 (sensor-limited) =================

float co2 = NAN;
float co2Smooth = NAN;

// ================= AGGREGATION =================
// aggregation window start (wall clock)
time_t aggStartTs = 0;

// ================= TIMING =================
uint32_t lastSample = 0;
uint32_t lastAgg = 0;
uint32_t lastTry = 0;

// ================= LD1020 motion sensor =================
constexpr uint8_t PIN_LD1020 = 5;

uint32_t lastMotionTs = 0;
constexpr uint32_t MOTION_HOLD_MS = 10000; // 10 s

struct LD1020Status
{
  bool present = false;
  bool motion = false;     // current state
  bool lastMotion = false; // previous state
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

  float fraction() const
  {
    return totalCount ? float(motionCount) / totalCount : 0.0f;
  }

  void reset() { motionCount = totalCount = 0; }
};

LD1020Agg ld1020Agg;

// ================= HW / NETWORK =================

AsyncWebServer server(80);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Adafruit_SHT4x sht4;
Adafruit_BMP280 bmp;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
SCD4x scd4;

// ================= LED =================

constexpr uint8_t PIN_G = 14;
constexpr uint8_t PIN_Y = 13;
constexpr uint8_t PIN_R = 27;

constexpr uint32_t PWM_FREQ = 1000;
constexpr uint8_t PWM_RES = 8;

void setupWDT()
{
  esp_task_wdt_config_t cfg = {
      .timeout_ms = WDT_TIMEOUT * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true};

  esp_task_wdt_init(&cfg);
  esp_task_wdt_add(NULL);
}

enum LedState
{
  LED_LOW,
  LED_MID,
  LED_HIGH
};

LedState co2LedState = LED_LOW;

////////////////////////////////////////////////////////////
// SENSOR STATE
////////////////////////////////////////////////////////////
struct SensorStatus
{
  bool present = false;
  bool initialized = false;
};

SensorStatus shtStat;
SensorStatus bmpStat;
SensorStatus tslStat;
SensorStatus scdStat;

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
  float min = 100000;
  float max = -100000;
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
    min = 100000;
    max = -100000;
    count = 0;
  }
};

AggMinMax tempAgg;
AggMinMax humAgg;
AggMean presAgg;
AggMean luxAgg;
AggMean co2Agg;

////////////////////////////////////////////////////////////
// I2C SCAN
////////////////////////////////////////////////////////////

bool isKnownAddr(uint8_t addr)
{
  return addr == 0x44 || addr == 0x76 || addr == 0x77 || addr == 0x29 || addr == 0x62;
}

void scanI2C()
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

      Serial.printf("[I2C] 0x%02X  %s\n",
                    addr,
                    isKnownAddr(addr) ? "KNOWN" : "UNKNOWN ⚠");

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

////////////////////////////////////////////////////////////
// SENSOR DETECT
////////////////////////////////////////////////////////////

void detectSensors()
{

  Serial.println("[BOOT] Detecting sensors...");

  // SHT
  shtStat.present = i2cDevices[0x44];
  if (shtStat.present)
  {
    Serial.print("[SHT4x] init... ");
    shtStat.initialized = sht4.begin(&Wire);
    Serial.println(shtStat.initialized ? "OK" : "FAIL");
  }

  // BMP
  uint8_t bmpAddr = 0;

  if (i2cDevices[0x76])
    bmpAddr = 0x76;
  else if (i2cDevices[0x77])
    bmpAddr = 0x77;

  bmpStat.present = bmpAddr != 0;

  if (bmpStat.present)
  {
    Serial.print("[BMP280] init... ");
    bmpStat.initialized = bmp.begin(bmpAddr);
    Serial.println(bmpStat.initialized ? "OK" : "FAIL");
  }

  // TSL
  tslStat.present = i2cDevices[0x29];
  if (tslStat.present)
  {
    Serial.print("[TSL2591] init... ");
    tslStat.initialized = tsl.begin();

    if (tslStat.initialized)
    {
      tsl.setGain(TSL2591_GAIN_MED);
      tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    }

    Serial.println(tslStat.initialized ? "OK" : "FAIL");
  }

  // SDC40
  scdStat.present = i2cDevices[0x62]; // SCD4x address

  if (scdStat.present)
  {
    Serial.print("[SCD4x] init... ");

    if (scd4.begin())
    {
      scdStat.initialized = true;

      scd4.startPeriodicMeasurement(); // IMPORTANT

      Serial.println("OK");
    }
    else
    {
      Serial.println("FAIL");
    }
  }

  Serial.println();
}

void setupLD1020()
{
  pinMode(PIN_LD1020, INPUT); // LD1020 output is digital high on motion
  ld1020.present = true;      // mark as present
  ld1020.motion = digitalRead(PIN_LD1020);
  ld1020.lastMotion = ld1020.motion;

  Serial.println("[LD1020] initialized on GPIO5");
}

////////////////////////////////////////////////////////////
// FILTER CORE
////////////////////////////////////////////////////////////

struct EMA
{
  float tau; // time constant [s]
  float value;

  EMA(float tau_sec) : tau(tau_sec), value(NAN) {}

  float update(float v, float dt)
  {
    const float a = dt / (tau + dt);

    if (isnan(value))
      value = v;
    else
      value += a * (v - value);

    return value;
  }
};

// ---------- history helper ----------
inline void push3(float *h, float v)
{
  h[0] = h[1];
  h[1] = h[2];
  h[2] = v;
}

////////////////////////////////////////////////////////////
// FILTER INSTANCES
////////////////////////////////////////////////////////////

// signal smoothing
EMA tempLPF(10.0f);
EMA humLPF(10.0f);
EMA luxLPF(5.0f);
EMA co2LPF(20.0f);

// gradient smoothing
EMA tempGradLPF(120.0f);
EMA humGradLPF(120.0f);
EMA co2GradLPF(180.0f);

// derivative history buffers
float tempHist[3] = {NAN, NAN, NAN};
float humHist[3] = {NAN, NAN, NAN};
float co2Hist[3] = {NAN, NAN, NAN};

////////////////////////////////////////////////////////////
// FAST SENSORS
////////////////////////////////////////////////////////////

void readFastSensors()
{
  const float dt = SAMPLE_INTERVAL / 1000.0f;

  // ---------- LD1020 ----------
  if (ld1020.present)
  {
    ld1020.motion = digitalRead(PIN_LD1020);
    ld1020Agg.add(ld1020.motion);

    // Track last motion timestamp
    if (ld1020.motion)
    {
      lastMotionTs = millis();
    }

    // Optional: detect rising edge
    if (ld1020.motion && !ld1020.lastMotion)
    {
      Serial.println("[LD1020] motion detected!");
    }

    ld1020.lastMotion = ld1020.motion;
  }

  // ---------- SHT4x ----------
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

      // central derivative (low noise)
      if (!isnan(tempHist[0]))
      {
        float g = (tempHist[2] - tempHist[0]) / (2.0f * dt);
        tempGrad = tempGradLPF.update(g, dt);
      }

      if (!isnan(humHist[0]))
      {
        float g = (humHist[2] - humHist[0]) / (2.0f * dt);
        humGrad = humGradLPF.update(g, dt);
      }

      tempAgg.add(tempSmooth);
      humAgg.add(humSmooth);
    }
  }

  // ---------- BMP280 ----------
  if (bmpStat.initialized)
  {
    pres = bmp.readPressure() / 100.0f;
    presAgg.add(pres);
  }

  // ---------- TSL2591 ----------
  if (tslStat.initialized)
  {
    sensors_event_t light;
    tsl.getEvent(&light);

    if (!isnan(light.light) &&
        light.light > 0 &&
        light.light < 200000)
    {
      lux = luxLPF.update(light.light, dt);
      luxAgg.add(lux);
    }
  }
}

////////////////////////////////////////////////////////////
// SCD40 CO2 SENSOR
////////////////////////////////////////////////////////////

void readSCD40()
{
  static uint32_t lastSample = 0;
  static uint32_t lastTs = 0;

  uint32_t now = millis();

  if (!scdStat.initialized)
    return;

  if (now - lastSample < SAMPLE_INTERVAL_SCD40)
    return;

  lastSample = now;

  if (!scd4.readMeasurement())
    return;

  float newCO2 = scd4.getCO2();

  if (newCO2 <= 0 || newCO2 > 10000)
    return;

  float dt =
      (lastTs == 0)
          ? SAMPLE_INTERVAL_SCD40 / 1000.0f
          : (now - lastTs) / 1000.0f;

  lastTs = now;

  co2 = newCO2;

  co2Smooth = co2LPF.update(co2, dt);

  push3(co2Hist, co2Smooth);

  // central derivative
  if (!isnan(co2Hist[0]))
  {
    float g = (co2Hist[2] - co2Hist[0]) / (2.0f * dt);
    co2Grad = co2GradLPF.update(g, dt);
  }
}

////////////////////////////////////////////////////////////
// READ ALL SENSORS
////////////////////////////////////////////////////////////

void readSensors()
{
  readFastSensors();
  readSCD40();
  Serial.println(digitalRead(PIN_LD1020));
}

////////////////////////////////////////////////////////////
// SYSTEM INFO
////////////////////////////////////////////////////////////

void printSystemInfo()
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
// WIFI WATCHDOG
////////////////////////////////////////////////////////////

uint32_t wifiLostAt = 0;

void wifiWatchdog()
{

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiLostAt = 0;
    return;
  }

  if (wifiLostAt == 0)
    wifiLostAt = millis();

  if (millis() - wifiLostAt > 30000)
  {
    Serial.println("WiFi stalled -> reboot");
    ESP.restart();
  }
}

////////////////////////////////////////////////////////////
// WIFI
////////////////////////////////////////////////////////////

void connectWiFi()
{

  if (WiFi.status() == WL_CONNECTED)
    return;

  Serial.print("WiFi connecting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint8_t tries = 0;

  while (WiFi.status() != WL_CONNECTED && tries < 30)
  {
    delay(1000);
    esp_task_wdt_reset(); // ✅ keep WDT alive
    Serial.print(".");
    tries++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("IP ");
    Serial.println(WiFi.localIP());
  }
}

////////////////////////////////////////////////////////////
// NTP
////////////////////////////////////////////////////////////

void setupTime()
{

  configTzTime(TZ_INFO, NTP_SERVER);

  time_t now = time(nullptr);
  uint32_t start = millis();

  Serial.print("NTP syncing");

  while (now < 1000000000 && millis() - start < 15000)
  {
    delay(1000);
    esp_task_wdt_reset(); // ✅
    Serial.print(".");
    now = time(nullptr);
  }

  if (now > 1000000000)
    Serial.println("\nTime OK");
  else
    Serial.println("\nNTP FAILED");
}

////////////////////////////////////////////////////////////
// LED OUTPUT
////////////////////////////////////////////////////////////
void updateLED()
{
  const uint32_t now = millis();

  ////////////////////////////////////////////////////////////
  // MOTION GATE
  // LED OFF if motion is too old
  ////////////////////////////////////////////////////////////

  if (lastMotionTs == 0 || (now - lastMotionTs) > MOTION_HOLD_MS)
  {
    ledcWrite(PIN_G, 0);
    ledcWrite(PIN_Y, 0);
    ledcWrite(PIN_R, 0);
    return;
  }

  ////////////////////////////////////////////////////////////
  // CO2 VALIDITY
  ////////////////////////////////////////////////////////////

  if (isnan(co2Smooth))
    return;

  static uint32_t lastUpdate = 0;
  static uint32_t lastBlink = 0;
  static bool blink = false;

  uint8_t g = 0, y = 0, r = 0;
  float co2 = co2Smooth;

  ////////////////////////////////////////////////////////////
  // CRITICAL (>1600 ppm) — fast blink
  ////////////////////////////////////////////////////////////

  if (co2 > 1600)
  {
    if (now - lastBlink >= 200) // 5 Hz
    {
      lastBlink = now;
      blink = !blink;
    }

    r = blink ? 255 : 0;

    ledcWrite(PIN_G, 0);
    ledcWrite(PIN_Y, 0);
    ledcWrite(PIN_R, r);
    return;
  }

  ////////////////////////////////////////////////////////////
  // WARNING (>1400 ppm) — slow blink
  ////////////////////////////////////////////////////////////

  if (co2 > 1400)
  {
    if (now - lastBlink >= 500) // 2 Hz
    {
      lastBlink = now;
      blink = !blink;
    }

    r = blink ? 255 : 0;

    ledcWrite(PIN_G, 0);
    ledcWrite(PIN_Y, 0);
    ledcWrite(PIN_R, r);
    return;
  }

  ////////////////////////////////////////////////////////////
  // NORMAL MODE (rate limited)
  ////////////////////////////////////////////////////////////

  if (now - lastUpdate < SAMPLE_INTERVAL)
    return;

  lastUpdate = now;

  if (co2 < 600)
  {
    g = 255;
  }
  else if (co2 < 800)
  {
    float t = (co2 - 600.0f) / 200.0f;
    g = (uint8_t)((1.0f - t) * 255);
    y = (uint8_t)(t * 255);
  }
  else if (co2 < 1000)
  {
    y = 255;
  }
  else if (co2 < 1200)
  {
    float t = (co2 - 1000.0f) / 200.0f;
    y = (uint8_t)((1.0f - t) * 255);
    r = (uint8_t)(t * 255);
  }
  else
  {
    r = 255;
  }

  ledcWrite(PIN_G, g);
  ledcWrite(PIN_Y, y);
  ledcWrite(PIN_R, r);

  ////////////////////////////////////////////////////////////
  // RATE-LIMITED LOGGING
  ////////////////////////////////////////////////////////////

  if (SERIAL_PRINT_VALUES)
  {
    static uint32_t lastLog = 0;

    if (now - lastLog > 5000)
    {
      lastLog = now;
      Serial.printf("[LED] CO2=%.1f G=%u Y=%u R=%u\n",
                    co2, g, y, r);
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
    bool retained = false)
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
      MQTT_DEVICE_ID,
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
void publishAgg()
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

  //--------------------------------------------------------
  // RESET AGGREGATORS
  //--------------------------------------------------------
  tempAgg.reset();
  humAgg.reset();
  presAgg.reset();
  luxAgg.reset();
  ld1020Agg.reset();
  co2Agg.reset(); // IMPORTANT for gradient validity

  //--------------------------------------------------------
  // START NEW AGGREGATION WINDOW
  //--------------------------------------------------------
  aggStartTs = time(nullptr);
  lastAgg = millis();

  //--------------------------------------------------------
  // ---- SERIAL PRINT ALL ----
  //--------------------------------------------------------
  if (SERIAL_PRINT_VALUES)
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
void connectMQTT()
{
  if (mqtt.connected())
    return;

  if (millis() - lastTry < 5000)
    return;

  lastTry = millis();

  Serial.print("MQTT connecting... ");

  //------------------------------------------------------
  // Build LWT topic
  //------------------------------------------------------
  char topicStatus[64];
  snprintf(
      topicStatus,
      sizeof(topicStatus),
      "%s/system/status",
      MQTT_DEVICE_ID);

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
          MQTT_DEVICE_ID,
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
// WEB JSON
////////////////////////////////////////////////////////////
String jsonData()
{

  auto safe = [](float v)
  {
    return isnan(v) || isinf(v) ? 0.0f : v;
  };

  char buf[1024];
  snprintf(buf, sizeof(buf),
           "{\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,\"lux\":%.2f,"
           "\"temp_smooth\":%.2f,\"hum_smooth\":%.2f,"
           "\"temp_grad\":%.3f,\"hum_grad\":%.3f,"
           "\"co2\":%.0f,\"co2_smooth\":%.0f,\"co2_grad\":%.3f,"
           "\"ld1020_motion\":%d,"
           "\"wifi_rssi\":%d,\"wifi_ch\":%d,"
           "\"heap\":%u,\"heap_min\":%u,"
           "\"uptime\":%lu,\"ip\":\"%s\",\"mqtt\":%s}",
           safe(temp), safe(hum), safe(pres), safe(lux),
           safe(tempSmooth), safe(humSmooth),
           safe(tempGrad), safe(humGrad),
           safe(co2), safe(co2Smooth), safe(co2Grad),
           ld1020.motion ? 1 : 0,
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
  ArduinoOTA.setHostname("esp32-lab");
  ArduinoOTA.setPasswordHash("");
  // ArduinoOTA.setPassword(nullptr); // disable password prompt
  ArduinoOTA.onStart([]()
                     {
    otaInProgress = true;
    esp_task_wdt_delete(NULL);   // pause watchdog for loopTask
    Serial.println("Start OTA, pausing sensors"); });

  ArduinoOTA.onEnd([]()
                   {
    otaInProgress = false;
    esp_task_wdt_add(NULL);      // re-enable watchdog
    Serial.println("\nEnd OTA"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress * 100) / total); });
  ArduinoOTA.onError([](ota_error_t error)
                     { Serial.printf("Error[%u]: ", error); });

  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////

void setup()
{

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  setupWDT();
  Serial.println("\n===== BOOT =====");
  Serial.printf("Reset reason: %d\n", esp_reset_reason());
  Wire.setClock(100000); // reduce I2C speed
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);

  // LED output PWM
  ledcAttach(PIN_G, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_Y, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_R, PWM_FREQ, PWM_RES);
  ledcAttachChannel(PIN_G, PWM_FREQ, PWM_RES, 0);
  ledcAttachChannel(PIN_Y, PWM_FREQ, PWM_RES, 1);
  ledcAttachChannel(PIN_R, PWM_FREQ, PWM_RES, 2);

  connectWiFi();
  setupTime();

  scanI2C();
  detectSensors();
  setupLD1020();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  printSystemInfo();

  startWeb();
  setupOTA();
}

////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////
void loop()
{
  esp_task_wdt_reset();
  wifiWatchdog();

  connectWiFi();
  connectMQTT();

  mqtt.loop();
  ArduinoOTA.handle();

  uint32_t now = millis();

  // --- SENSOR UPDATE (slow) ---
  if (!otaInProgress && (now - lastSample >= SAMPLE_INTERVAL))
  {
    lastSample += SAMPLE_INTERVAL;
    readSensors();
  }

  // --- AGG ---
  if (!otaInProgress && (now - lastAgg > AGG_INTERVAL))
  {
    lastAgg = now;
    publishAgg();
  }

  // --- LED RENDER (fast, every loop) ---
  updateLED();

  ets_delay_us(1000); // 1 ms microsleep
}
