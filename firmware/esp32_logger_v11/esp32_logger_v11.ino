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

#define WDT_TIMEOUT 120 // watchdog timeout in seconds

#define NTP_SERVER "tak.cesnet.cz"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// ================= CONFIG =================

#define SAMPLE_INTERVAL 500        // 0.5 s sampling
#define SAMPLE_INTERVAL_SCD40 5000 // 5 s sampling for SCD40 (slow sensor)
#define AGG_INTERVAL 300000        // 5 min aggregation
#define FILTER_N 5                 // 5 s running mean
#define SERIAL_PRINT_VALUES 0

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

// unified control signal (LED etc.)
float g_co2 = NAN;

// ================= HW / NETWORK =================

AsyncWebServer server(80);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Adafruit_SHT4x sht4;
Adafruit_BMP280 bmp;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
SCD4x scd4;

// ================= TIMING =================

uint32_t lastSample = 0;
uint32_t lastAgg = 0;
uint32_t lastTry = 0;

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

// ================= SENSOR READ =================
// ================= EMA =================
struct EMA
{
  float alpha;
  float value;
  EMA(float a) : alpha(a), value(NAN) {}
  float update(float v)
  {
    if (isnan(value))
      value = v;
    else
      value = alpha * v + (1 - alpha) * value;
    return value;
  }
};

// ================= EMA INSTANCES =================
EMA tempEMA(0.3f);
EMA humEMA(0.3f);
EMA luxEMA(0.2f);
EMA co2EMA(0.3f);

// ================= READ FAST SENSORS =================
void readFastSensors()
{
  const float dt = SAMPLE_INTERVAL / 1000.0;
  const float alphaGrad = 0.3f;

  // ---- SHT ----
  if (shtStat.initialized)
  {
    sensors_event_t h, t;
    if (sht4.getEvent(&h, &t))
    {
      temp = t.temperature;
      hum = h.relative_humidity;

      tempSmooth = tempF.add(temp);
      humSmooth = humF.add(hum);

      float tempE = tempEMA.update(tempSmooth);
      float humE = humEMA.update(humSmooth);

      if (!isnan(tempPrev))
      {
        float rawGrad = (tempE - tempPrev) / dt;
        tempGrad = isnan(tempGrad) ? rawGrad : alphaGrad * rawGrad + (1 - alphaGrad) * tempGrad;
      }
      if (!isnan(humPrev))
      {
        float rawGrad = (humE - humPrev) / dt;
        humGrad = isnan(humGrad) ? rawGrad : alphaGrad * rawGrad + (1 - alphaGrad) * humGrad;
      }

      tempPrev = tempE;
      humPrev = humE;

      tempAgg.add(tempE);
      humAgg.add(humE);
    }
  }

  // ---- BMP ----
  if (bmpStat.initialized)
  {
    pres = bmp.readPressure() / 100.0;
    presAgg.add(pres);
  }

  // ---- TSL ----
  if (tslStat.initialized)
  {
    sensors_event_t light;
    tsl.getEvent(&light);
    if (!isnan(light.light) && light.light > 0 && light.light < 200000)
    {
      lux = light.light;
      luxSmooth = luxF.add(lux);
      luxEMA.update(luxSmooth);
      luxAgg.add(luxSmooth);
    }
  }
}

// ================= READ SLOW SENSOR (SCD4x CO2) =================
void readSCD40()
{
  static uint32_t lastSampleSCD = 0;
  uint32_t now = millis();
  const float alphaGrad = 0.3f;

  if (!scdStat.initialized)
    return;
  if (now - lastSampleSCD < SAMPLE_INTERVAL_SCD40)
    return;

  lastSampleSCD = now;

  if (!scd4.readMeasurement())
    return;

  float newCO2 = scd4.getCO2();
  if (newCO2 <= 0 || newCO2 >= 10000)
    return;

  co2 = newCO2;
  co2Smooth = co2EMA.update(co2);

  static float co2PrevLocal = NAN;
  static uint32_t co2LastTs = 0;

  if (!isnan(co2PrevLocal) && co2LastTs > 0)
  {
    float dt_real = (now - co2LastTs) / 1000.0f;
    if (dt_real > 0.1f)
    {
      float rawGrad = (co2Smooth - co2PrevLocal) / dt_real;
      co2Grad = isnan(co2Grad) ? rawGrad : alphaGrad * rawGrad + (1 - alphaGrad) * co2Grad;
    }
  }

  co2PrevLocal = co2Smooth;
  co2LastTs = now;

  g_co2 = co2Smooth;
}

// ================= READ ALL =================
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
  if (isnan(g_co2))
    return;
  static uint32_t lastUpdate = 0;
  static uint32_t lastBlink = 0;
  static bool blink = false;

  const uint32_t now = millis();

  uint8_t g = 0, y = 0, r = 0;

  float co2 = g_co2;

  // ---------- CRITICAL: fast blink (>1600) ----------
  if (co2 > 1600)
  {
    if (now - lastBlink >= 200)
    { // 5 Hz
      lastBlink = now;
      blink = !blink;
      Serial.printf("[LED] CO2=%.1f G=%u Y=%u R=%u\n", co2, g, y, r);
    }

    r = blink ? 255 : 0;

    ledcWrite(PIN_G, 0);
    ledcWrite(PIN_Y, 0);
    ledcWrite(PIN_R, r);
    return;
  }

  // ---------- WARNING: slow blink (>1400) ----------
  if (co2 > 1400)
  {
    if (now - lastBlink >= 500)
    { // 2 Hz
      lastBlink = now;
      blink = !blink;
      Serial.printf("[LED] CO2=%.1f G=%u Y=%u R=%u\n", co2, g, y, r);
    }

    r = blink ? 255 : 0;

    ledcWrite(PIN_G, 0);
    ledcWrite(PIN_Y, 0);
    ledcWrite(PIN_R, r);
    return;
  }

  // ---------- NORMAL: continuous mixing ----------
  if (now - lastUpdate < SAMPLE_INTERVAL)
    return; // updated at SAMPLE_INTERVAL

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

  //~ // --- rate-limited logging ---
  if (SERIAL_PRINT_VALUES)
  {
    static uint32_t lastLog = 0;
    if (now - lastLog > 1000)
    {
      lastLog = now;
      Serial.printf("[LED] CO2=%.1f G=%u Y=%u R=%u\n", co2, g, y, r);
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
    const char* sensor,
    const char* metric,
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
        (uint8_t*)payload,
        strlen(payload),
        retained);
}

////////////////////////////////////////////////////////////
// SEND ALL SENSOR VALUES
////////////////////////////////////////////////////////////
void mqttSendAll()
{
    unsigned long ts = time(nullptr);

    // ---- SHT40 ----
    mqttSendCSV(ts,"sht40","temp",temp);
    mqttSendCSV(ts,"sht40","temp_smooth",tempSmooth);
    mqttSendCSV(ts,"sht40","temp_grad",tempGrad);

    mqttSendCSV(ts,"sht40","hum",hum);
    mqttSendCSV(ts,"sht40","hum_smooth",humSmooth);
    mqttSendCSV(ts,"sht40","hum_grad",humGrad);

    // ---- CO2 ----
    mqttSendCSV(ts,"scd40","co2",co2);
    mqttSendCSV(ts,"scd40","co2_smooth",co2Smooth);
    mqttSendCSV(ts,"scd40","co2_grad",co2Grad);

    // ---- Environment ----
    mqttSendCSV(ts,"bmp280","pressure",pres);
    mqttSendCSV(ts,"tsl2591","lux",lux);
}

////////////////////////////////////////////////////////////
// PUBLISH AGGREGATED DATA
////////////////////////////////////////////////////////////
void publishAgg()
{
    if (!mqtt.connected())
        return;

    time_t ts_center = time(nullptr);

    if (lastAgg >= AGG_INTERVAL)
        ts_center -= AGG_INTERVAL / 2000;

    // ---------- Temperature ----------
    if (tempAgg.count)
    {
        mqttSendCSV(ts_center,"sht40","temp_mean",tempAgg.mean());
        mqttSendCSV(ts_center,"sht40","temp_min",tempAgg.min);
        mqttSendCSV(ts_center,"sht40","temp_max",tempAgg.max);
    }

    // ---------- Humidity ----------
    if (humAgg.count)
    {
        mqttSendCSV(ts_center,"sht40","rh_mean",humAgg.mean());
        mqttSendCSV(ts_center,"sht40","rh_min",humAgg.min);
        mqttSendCSV(ts_center,"sht40","rh_max",humAgg.max);
    }

    // ---------- Pressure ----------
    if (presAgg.count)
        mqttSendCSV(ts_center,"bmp280","pressure_mean",presAgg.mean());

    // ---------- Light ----------
    if (luxAgg.count)
        mqttSendCSV(ts_center,"tsl2591","lux_mean",luxAgg.mean());

    // ---------- Gradients ----------
    mqttSendCSV(ts_center,"sht40","temp_grad",tempGrad);
    mqttSendCSV(ts_center,"sht40","rh_grad",humGrad);

    // ---------- System ----------
    mqttSendCSV(ts_center,"system","wifi_rssi",WiFi.RSSI());
    mqttSendCSV(ts_center,"system","heap",ESP.getFreeHeap());
    mqttSendCSV(ts_center,"system","heap_min",ESP.getMinFreeHeap());
    mqttSendCSV(ts_center,"system","uptime",millis()/1000.0f);

    // ---------- CO2 ----------
    mqttSendCSV(ts_center,"scd40","co2_smooth",co2Smooth);
    mqttSendCSV(ts_center,"scd40","co2_grad",co2Grad);

    // reset aggregators
    tempAgg.reset();
    humAgg.reset();
    presAgg.reset();
    luxAgg.reset();
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
            topicStatus,   // LWT topic
            1,
            true,          // retain
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
            1.0f,
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
           "\"co2\":%.0f,"
           "\"co2_smooth\":%.0f,"
           "\"co2_grad\":%.3f,"
           "\"wifi_rssi\":%d,\"wifi_ch\":%d,"
           "\"heap\":%u,\"heap_min\":%u,"
           "\"uptime\":%lu,\"ip\":\"%s\",\"mqtt\":%s}",

           safe(temp), safe(hum), safe(pres), safe(lux),
           safe(tempSmooth), safe(humSmooth),
           safe(tempGrad), safe(humGrad),
           safe(co2), safe(co2Smooth), safe(co2Grad),

           WiFi.RSSI(), WiFi.channel(),
           ESP.getFreeHeap(), ESP.getMinFreeHeap(),
           millis() / 1000,
           WiFi.localIP().toString().c_str(),
           mqtt.connected() ? "true" : "false");

  return String(buf);
}
void startWeb()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
            {

        const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body { font-family:system-ui; background:#0e0e0e; color:#eee; margin:16px; }
h1 { margin-bottom:6px; font-size:20px; }
.grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(200px,1fr)); gap:8px; }
.card { background:#1a1a1a; padding:8px; border-radius:8px; }
canvas { width:100%; }
.stat { font-size:16px; margin-bottom:4px; }
.sys { margin-top:8px; font-size:13px; line-height:1.4; }
</style>
</head>
<body>

<h1>ESP32 Kitchen Lab</h1>
<div class="grid">

  <div class="card">
    <div class="stat">CO2: <span id="co2_val">--</span> ppm</div>
    <canvas id="co2Chart" height="180"></canvas>
  </div>

  <div class="card">
    <div class="stat">Temp: <span id="temp_val">--</span> °C</div>
    <canvas id="tempChart" height="180"></canvas>
  </div>

  <div class="card">
    <div class="stat">Humidity: <span id="hum_val">--</span> %</div>
    <canvas id="humChart" height="180"></canvas>
  </div>

  <div class="card">
    <div class="stat">dCO₂: <span id="co2grad_val">--</span> ppm/s</div>
    <canvas id="co2gradChart" height="180"></canvas>
  </div>

  <div class="card">
    <div class="stat">dT: <span id="tgrad_val">--</span> °C/s</div>
    <canvas id="tgradChart" height="180"></canvas>
  </div>

  <div class="card">
    <div class="stat">dRH: <span id="hgrad_val">--</span> %/s</div>
    <canvas id="hgradChart" height="180"></canvas>
  </div>
  
  <div class="card">
    <div class="stat">Pressure: <span id="pres_val">--</span> hPa</div>
    <canvas id="presChart" height="180"></canvas>
  </div>

  <div class="card">
    <div class="stat">Light: <span id="lux_val">--</span> lx</div>
    <canvas id="luxChart" height="180"></canvas>
  </div>


</div>

<div class="card sys">
<b>System health</b><br>
<div id="sys"></div>
</div>

<script>
const tempEl=document.getElementById("temp_val")
const humEl=document.getElementById("hum_val")
const presEl=document.getElementById("pres_val")
const luxEl=document.getElementById("lux_val")
const co2El=document.getElementById("co2_val")
const co2gradEl = document.getElementById("co2grad_val");
const tgradEl=document.getElementById("tgrad_val")
const hgradEl=document.getElementById("hgrad_val")
const sysEl=document.getElementById("sys")
const UPDATE_INTERVAL=5000 // ms, should match SAMPLE_INTERVAL_SCD40 in firmware
const HISTORY_DURATION=60*60*1000 // ms, 10 hours
const MAX_POINTS=HISTORY_DURATION/UPDATE_INTERVAL

function chart(id,color,min,max){
  return new Chart(
    document.getElementById(id),
    {
      type:"line",
      data:{
        labels:[],
        datasets:[
          { data:[], borderColor:color, borderWidth:1.5, pointRadius:0, tension:0.15 },
          { data:[], borderColor:"#888888", borderWidth:1, pointRadius:0, borderDash:[5,5], label:"zero", fill:false } // zero line
        ]
      },
      options:{
        responsive:false,
        animation:false,
        plugins:{legend:{display:false}},
        scales:{
          x:{display:false},
          y:{display:true, min:min, max:max}
        }
      }
    })
}

// Push function updated to handle zero line
function push(chart,v){
  const d = chart.data.datasets[0].data
  const z = chart.data.datasets[1].data
  const l = chart.data.labels
  d.push(v)
  z.push(0)        // always zero
  l.push("")
  if(d.length > MAX_POINTS){
    d.shift(); z.shift(); l.shift();
  }
  chart.update("none")
}

// Fixed ranges
const tempChart=chart("tempChart","#ff0000",10,30)
const humChart=chart("humChart","#0099ff",20,80)
const presChart=chart("presChart","#ffc857",900,1100)
const luxChart=chart("luxChart","#ffffff",0,500)  
const co2Chart=chart("co2Chart","#0dff00",400,1600)
const co2gradChart = chart("co2gradChart", "#aaffaa", -4., 4.);
const tgradChart=chart("tgradChart","#ffaaaa",-.02,.02)
const hgradChart=chart("hgradChart","#00eeff",-.1,.1)


async function update(){
  const r=await fetch("/data")
  const j=await r.json()
co2El.innerText = j.co2.toFixed(0);
tempEl.innerText = j.temp.toFixed(2);
humEl.innerText = j.hum.toFixed(2);
presEl.innerText = j.pres.toFixed(1);
luxEl.innerText = j.lux.toFixed(1);
co2gradEl.innerText = j.co2_grad.toFixed(3);
tgradEl.innerText = j.temp_grad.toFixed(3);
hgradEl.innerText = j.hum_grad.toFixed(3);

  push(tempChart,j.temp)
  push(humChart,j.hum)
  push(presChart,j.pres)
  push(luxChart,j.lux)
  push(co2Chart,j.co2)
  push(co2gradChart, j.co2_grad);
  push(tgradChart,j.temp_grad)
  push(hgradChart,j.hum_grad)

  sysEl.innerHTML=
    "WiFi RSSI: "+j.wifi_rssi+" dBm<br>"+
    "Heap: "+j.heap+" B<br>"+
    "Min heap: "+j.heap_min+" B<br>"+
    "Uptime: "+j.uptime+" s<br>"+
    "MQTT: "+j.mqtt
}

setInterval(update,UPDATE_INTERVAL)
</script>
</body>
</html>
)rawliteral";

        req->send_P(200,"text/html",page); });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req)
            { req->send(200, "application/json", jsonData()); });

  server.begin();
  Serial.println("[WEB] started");
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
