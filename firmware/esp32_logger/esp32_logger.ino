#include <WiFi.h>
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

#define WDT_TIMEOUT 120 // seconds

#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#define SAMPLE_INTERVAL 500        // 0.5 s
#define SAMPLE_INTERVAL_SCD40 5000 // 5 s
#define AGG_INTERVAL 300000        // 5 min
#define FILTER_N 5
#define SERIAL_PRINT_VALUES 1

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
constexpr uint32_t MOTION_HOLD_MS = 10000; // time to hold LED on after motion detected (ms)

constexpr uint8_t PIN_AM312 = 19;
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

constexpr uint8_t PIN_LD1020 = 5;

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

constexpr uint8_t PIN_G = 14, PIN_Y = 13, PIN_R = 27;
constexpr uint32_t PWM_FREQ = 1000;
constexpr uint8_t PWM_RES = 8;
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

void setupWDT();
void scanI2C();
bool isKnownAddr(uint8_t addr);
void detectSensors();
void setupLD1020();
void setupAM312();
void readFastSensors();
void readSCD40();
void readSensors();
void printSystemInfo();
void wifiWatchdog();
void connectWiFi();
void setupTime();
void updateLED();
void mqttSendCSV(unsigned long ts, const char *sensor, const char *metric, float value, bool retained = false);
void publishAgg();

////////////////////////////////////////////////////////////
// IMPLEMENTATION
////////////////////////////////////////////////////////////

void setupWDT()
{
  esp_task_wdt_config_t cfg = {.timeout_ms = WDT_TIMEOUT * 1000, .idle_core_mask = 0, .trigger_panic = true};
  esp_task_wdt_init(&cfg);
  esp_task_wdt_add(NULL);
}

bool isKnownAddr(uint8_t addr) { return addr == 0x44 || addr == 0x76 || addr == 0x77 || addr == 0x29 || addr == 0x62; }

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
      Serial.printf("[I2C] 0x%02X  %s\n", addr, isKnownAddr(addr) ? "KNOWN" : "UNKNOWN ⚠");
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

void detectSensors()
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

void setupLD1020()
{
  pinMode(PIN_LD1020, INPUT);
  ld1020.present = true;
  ld1020.motion = digitalRead(PIN_LD1020);
  ld1020.lastMotion = ld1020.motion;
  Serial.println("[LD1020] initialized on GPIO5");
}
void setupAM312()
{
  pinMode(PIN_AM312, INPUT);
  am312.motion = digitalRead(PIN_AM312);
  am312.lastMotion = am312.motion;
  Serial.println("[AM312] initialized on GPIO19");
}

void readFastSensors()
{
  const float dt = SAMPLE_INTERVAL / 1000.0f;
  if (ld1020.present)
  {
    ld1020.motion = digitalRead(PIN_LD1020);
    ld1020Agg.add(ld1020.motion);
    if (ld1020.motion)
      ld1020.lastMotionTs = millis();
    if (ld1020.motion && !ld1020.lastMotion)
      Serial.println("[LD1020] motion detected!");
    ld1020.lastMotion = ld1020.motion;
  }
  if (am312.present)
  {
    am312.motion = digitalRead(PIN_AM312);
    am312Agg.add(am312.motion);
    if (am312.motion)
      am312.lastMotionTs = millis();
    if (am312.motion && !am312.lastMotion)
      Serial.println("[AM312] motion detected!");
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
}

void readSCD40()
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
}

void readSensors()
{
  readFastSensors();
  readSCD40();
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
  uint32_t lastMotionTs = max(ld1020.lastMotionTs, am312.lastMotionTs);
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

  char buf[1280]; // increased size for additional field

  snprintf(buf, sizeof(buf),
           "{\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,\"lux\":%.2f,"
           "\"temp_smooth\":%.2f,\"hum_smooth\":%.2f,"
           "\"temp_grad\":%.3f,\"hum_grad\":%.3f,"
           "\"co2\":%.0f,\"co2_smooth\":%.0f,\"co2_grad\":%.3f,"
           "\"ld1020_motion\":%d,\"am312_motion\":%d,"
           "\"wifi_rssi\":%d,\"wifi_ch\":%d,"
           "\"heap\":%u,\"heap_min\":%u,"
           "\"uptime\":%lu,\"ip\":\"%s\",\"mqtt\":%s}",
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
