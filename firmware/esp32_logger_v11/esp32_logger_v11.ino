#include <WiFi.h>
#include <Wire.h>

#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "wifi_secrets.h"

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
#define MQTT_CLIENT_TYPE "sensor"
#define MQTT_CLIENT_ID   "kit_lab"

#define MQTT_BASE MQTT_CLIENT_TYPE "/" MQTT_CLIENT_ID
#define TOPIC_STATUS MQTT_BASE "/status"

#define I2C_SDA 21
#define I2C_SCL 22

#define WDT_TIMEOUT 30

#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// ================= CONFIG =================

#define SAMPLE_INTERVAL 500      // 0.5 s sampling
#define AGG_INTERVAL 300000       // 5 min aggregation
#define FILTER_N 5                // 5 s running mean
#define SERIAL_PRINT_VALUES 0

// ================= RUNNING MEAN =================
template<int N>
struct RunningMean {
  float buf[N] = {0};
  int idx = 0;
  int count = 0;
  float sum = 0;

  float add(float v) {
    if (count < N) {
      buf[idx] = v;
      sum += v;
      count++;
    } else {
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
float hum  = NAN;
float pres = NAN;
float lux  = NAN;

// ================= FILTERED VALUES =================

float tempSmooth = NAN;
float humSmooth  = NAN;
float luxSmooth  = NAN;

// ================= DERIVATIVES =================

float tempPrev = NAN;
float humPrev  = NAN;
float co2Prev  = NAN;

float tempGrad = 0;
float humGrad  = 0;
float co2Grad  = 0;

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
uint32_t lastAgg    = 0;
uint32_t lastTry    = 0;

// ================= LED =================

constexpr uint8_t PIN_G = 14;
constexpr uint8_t PIN_Y = 13;
constexpr uint8_t PIN_R = 27;

constexpr uint32_t PWM_FREQ = 1000;
constexpr uint8_t PWM_RES   = 8;

void setupWDT() {
  esp_task_wdt_config_t cfg = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  esp_task_wdt_init(&cfg);
  esp_task_wdt_add(NULL);
}


enum LedState {
  LED_LOW,
  LED_MID,
  LED_HIGH
};

LedState co2LedState = LED_LOW;

////////////////////////////////////////////////////////////
// SENSOR STATE
////////////////////////////////////////////////////////////
struct SensorStatus {
  bool present=false;
  bool initialized=false;
};

SensorStatus shtStat;
SensorStatus bmpStat;
SensorStatus tslStat;
SensorStatus scdStat;

bool i2cDevices[128]={false};




////////////////////////////////////////////////////////////
// AGGREGATION
////////////////////////////////////////////////////////////

struct AggMean{
  float sum=0;
  uint32_t count=0;
  void add(float v){ sum+=v; count++; }
  float mean() const{ return count?sum/count:NAN; }
  void reset(){ sum=0; count=0; }
};

struct AggMinMax{
  float sum=0;
  float min=100000;
  float max=-100000;
  uint32_t count=0;

  void add(float v){
    sum+=v; count++;
    if(v<min) min=v;
    if(v>max) max=v;
  }

  float mean() const{ return count?sum/count:NAN; }

  void reset(){
    sum=0; min=100000; max=-100000; count=0;
  }
};

AggMinMax tempAgg;
AggMinMax humAgg;
AggMean presAgg;
AggMean luxAgg;



////////////////////////////////////////////////////////////
// I2C SCAN
////////////////////////////////////////////////////////////

bool isKnownAddr(uint8_t addr){
  return addr==0x44 || addr==0x76 || addr==0x77 || addr==0x29 || addr==0x62 ;
}

void scanI2C(){

  Serial.println("\n========== I2C SCAN ==========");

  uint8_t found=0;

  for(uint8_t addr=1; addr<127; addr++){

    Wire.beginTransmission(addr);
    uint8_t err=Wire.endTransmission();

    if(err==0){

      i2cDevices[addr]=true;

      Serial.printf("[I2C] 0x%02X  %s\n",
                    addr,
                    isKnownAddr(addr) ? "KNOWN" : "UNKNOWN ⚠");

      found++;

    }else if(err==4){
      Serial.printf("[I2C] 0x%02X ERROR\n",addr);
    }
  }

  Serial.printf("[I2C] Total: %u\n",found);
  Serial.println("===============================\n");
}

////////////////////////////////////////////////////////////
// SENSOR DETECT
////////////////////////////////////////////////////////////

void detectSensors(){

  Serial.println("[BOOT] Detecting sensors...");

  // SHT
  shtStat.present = i2cDevices[0x44];
  if(shtStat.present){
    Serial.print("[SHT4x] init... ");
    shtStat.initialized = sht4.begin(&Wire);
    Serial.println(shtStat.initialized?"OK":"FAIL");
  }

  // BMP
uint8_t bmpAddr = 0;

if (i2cDevices[0x76]) bmpAddr = 0x76;
else if (i2cDevices[0x77]) bmpAddr = 0x77;

bmpStat.present = bmpAddr != 0;

if (bmpStat.present) {
  Serial.print("[BMP280] init... ");
  bmpStat.initialized = bmp.begin(bmpAddr);
  Serial.println(bmpStat.initialized ? "OK" : "FAIL");
}

  // TSL
  tslStat.present = i2cDevices[0x29];
  if(tslStat.present){
    Serial.print("[TSL2591] init... ");
    tslStat.initialized = tsl.begin();

    if(tslStat.initialized){
      tsl.setGain(TSL2591_GAIN_MED);
      tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    }

    Serial.println(tslStat.initialized?"OK":"FAIL");
  }

    // SDC40
    scdStat.present = i2cDevices[0x62];   // SCD4x address

    if (scdStat.present) {
      Serial.print("[SCD4x] init... ");

      if (scd4.begin()) {
        scdStat.initialized = true;

        scd4.startPeriodicMeasurement();   // IMPORTANT

        Serial.println("OK");
      } else {
        Serial.println("FAIL");
      }
    }


  Serial.println();
}


// ================= SENSOR READ =================

void readSensors() {

  const float dt = SAMPLE_INTERVAL / 1000.0;

  // ---- SHT ----
  if (shtStat.initialized) {
    sensors_event_t h, t;

    if (sht4.getEvent(&h, &t)) {

      temp = t.temperature;
      hum  = h.relative_humidity;

      tempSmooth = tempF.add(temp);
      humSmooth  = humF.add(hum);

      if (!isnan(tempPrev))
        tempGrad = (tempSmooth - tempPrev) / dt;

      if (!isnan(humPrev))
        humGrad = (humSmooth - humPrev) / dt;

      tempPrev = tempSmooth;
      humPrev  = humSmooth;

      // aggregation uses filtered values
      tempAgg.add(tempSmooth);
      humAgg.add(humSmooth);
    }
  }

  // ---- BMP ----
  if (bmpStat.initialized) {
    pres = bmp.readPressure() / 100.0;
    presAgg.add(pres);
  }

  // ---- TSL ----
  if (tslStat.initialized) {
    sensors_event_t light;
    tsl.getEvent(&light);

    if (!isnan(light.light) && light.light > 0 && light.light < 200000) {
      lux = light.light;
      luxSmooth = luxF.add(lux);
      luxAgg.add(luxSmooth);
    }
  }

  // ---- SCD4x (true ~5 s updates) ----
  if (scdStat.initialized) {

static uint32_t co2LastTs = 0;

if (scd4.readMeasurement()) {

  uint32_t now = millis();

  float newCO2 = scd4.getCO2();

  if (newCO2 > 0 && newCO2 < 10000) {

    co2 = newCO2;

    if (isnan(co2Smooth))
      co2Smooth = co2;
    else
      co2Smooth = 0.3f * co2 + 0.7f * co2Smooth;

    if (!isnan(co2Prev) && co2LastTs > 0) {
      float dt_real = (now - co2LastTs) / 1000.0f;
      if (dt_real > 0.1f)  // guard
        co2Grad = (co2Smooth - co2Prev) / dt_real;
    }

    co2Prev = co2Smooth;
    co2LastTs = now;
  }
}
  }

  // unified control signal
  g_co2 = co2Smooth;
}




////////////////////////////////////////////////////////////
// SYSTEM INFO
////////////////////////////////////////////////////////////

void printSystemInfo(){

  Serial.println("========== SYSTEM ==========");

  Serial.printf("CPU: %u MHz\n",ESP.getCpuFreqMHz());
  Serial.printf("Heap: %u (min %u)\n",
                ESP.getFreeHeap(),
                ESP.getMinFreeHeap());

  Serial.printf("WiFi RSSI: %d dBm\n",WiFi.RSSI());
  Serial.printf("IP: %s\n",WiFi.localIP().toString().c_str());

  Serial.println("\nSensors:");

  Serial.printf("SHT4x   : %s / %s\n",
                shtStat.present?"present":"missing",
                shtStat.initialized?"OK":"FAIL");

  Serial.printf("BMP280  : %s / %s\n",
                bmpStat.present?"present":"missing",
                bmpStat.initialized?"OK":"FAIL");

  Serial.printf("TSL2591 : %s / %s\n",
                tslStat.present?"present":"missing",
                tslStat.initialized?"OK":"FAIL");

  Serial.printf("SCD4x : %s / %s\n",
                scdStat.present?"present":"missing",
                scdStat.initialized?"OK":"FAIL");


  Serial.println("============================\n");
}

////////////////////////////////////////////////////////////
// WIFI WATCHDOG
////////////////////////////////////////////////////////////

uint32_t wifiLostAt=0;

void wifiWatchdog(){

  if(WiFi.status()==WL_CONNECTED){
    wifiLostAt=0;
    return;
  }

  if(wifiLostAt==0)
    wifiLostAt=millis();

  if(millis()-wifiLostAt>30000){
    Serial.println("WiFi stalled -> reboot");
    ESP.restart();
  }
}

////////////////////////////////////////////////////////////
// WIFI
////////////////////////////////////////////////////////////

void connectWiFi(){

  if(WiFi.status()==WL_CONNECTED)
    return;

  Serial.print("WiFi connecting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID,WIFI_PASS);

  uint8_t tries=0;

while(WiFi.status()!=WL_CONNECTED && tries<30){
  delay(1000);
  esp_task_wdt_reset();   // ✅ keep WDT alive
  Serial.print(".");
  tries++;
}

  Serial.println();

  if(WiFi.status()==WL_CONNECTED){
    Serial.print("IP ");
    Serial.println(WiFi.localIP());
  }
}

////////////////////////////////////////////////////////////
// NTP
////////////////////////////////////////////////////////////

void setupTime(){

  configTzTime(TZ_INFO,NTP_SERVER);

  time_t now=time(nullptr);
  uint32_t start=millis();

  Serial.print("NTP syncing");

while(now<1000000000 && millis()-start<15000){
  delay(1000);
  esp_task_wdt_reset();   // ✅
  Serial.print(".");
  now=time(nullptr);
}

  if(now>1000000000)
    Serial.println("\nTime OK");
  else
    Serial.println("\nNTP FAILED");
}

////////////////////////////////////////////////////////////
// MQTT
////////////////////////////////////////////////////////////

void connectMQTT(){

  if(mqtt.connected())
    return;

  if(millis()-lastTry<5000)
    return;

  lastTry=millis();

  Serial.print("MQTT connecting... ");

  if(mqtt.connect(
        MQTT_CLIENT_ID,
        TOPIC_STATUS,
        1,
        true,
        "offline")){

    mqtt.publish(TOPIC_STATUS,"online",true);
    Serial.println("OK");

  } else{
    Serial.print("FAIL rc=");
    Serial.println(mqtt.state());
  }
}

void mqttSend(const char *sensor,const char *metric,float value,bool retained=true){

  if(!mqtt.connected()) return;
  if(isnan(value)||isinf(value)) return;

  char topic[96];
  char payload[32];

  snprintf(topic,sizeof(topic),MQTT_BASE "/%s/%s",sensor,metric);
  snprintf(payload,sizeof(payload),"%.3f",value);

  mqtt.publish(topic,payload,retained);
}


////////////////////////////////////////////////////////////
// LED OUTPUT
////////////////////////////////////////////////////////////


void updateLED() {
  if (isnan(g_co2)) return;
  static uint32_t lastUpdate = 0;
  static uint32_t lastBlink = 0;
  static bool blink = false;

  const uint32_t now = millis();

  uint8_t g = 0, y = 0, r = 0;

  float co2 = g_co2;

  // ---------- CRITICAL: fast blink (>1600) ----------
  if (co2 > 1600) {
    if (now - lastBlink >= 200) {   // 5 Hz
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
  if (co2 > 1400) {
    if (now - lastBlink >= 500) {   // 2 Hz
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
  if (now - lastUpdate < SAMPLE_INTERVAL) return; // updated at SAMPLE_INTERVAL

  lastUpdate = now;
  if (co2 < 600) {
    g = 255;
  }
  else if (co2 < 800) {
    float t = (co2 - 600.0f) / 200.0f;
    g = (uint8_t)((1.0f - t) * 255);
    y = (uint8_t)(t * 255);
  }
  else if (co2 < 1000) {
    y = 255;
  }
  else if (co2 < 1200) {
    float t = (co2 - 1000.0f) / 200.0f;
    y = (uint8_t)((1.0f - t) * 255);
    r = (uint8_t)(t * 255);
  }
  else {
    r = 255;
  }

  ledcWrite(PIN_G, g);
  ledcWrite(PIN_Y, y);
  ledcWrite(PIN_R, r);

  //~ // --- rate-limited logging ---
  if (SERIAL_PRINT_VALUES) {
  static uint32_t lastLog = 0;
  if (now - lastLog > 1000) {
    lastLog = now;
    Serial.printf("[LED] CO2=%.1f G=%u Y=%u R=%u\n", co2, g, y, r);
  }
}
}
////////////////////////////////////////////////////////////
// PUBLISH AGG
////////////////////////////////////////////////////////////

void publishAgg(){

  // send 5min mean values to mqtt

  if(!mqtt.connected()) return;

  if(tempAgg.count){
    // mqttSend("sht40","temp",temp);
    mqttSend("sht40","temp_mean",tempAgg.mean());
    mqttSend("sht40","temp_min",tempAgg.min);
    mqttSend("sht40","temp_max",tempAgg.max);
  }

  if(humAgg.count){
    // mqttSend("sht40","rh",hum);
    mqttSend("sht40","rh_mean",humAgg.mean());
    mqttSend("sht40","rh_min",humAgg.min);
    mqttSend("sht40","rh_max",humAgg.max);
  }

  if(presAgg.count)
    mqttSend("bmp280","pressure_mean",presAgg.mean());
    // mqttSend("bmp280","pressure",pres);

  if(luxAgg.count)
    // mqttSend("tsl2591","lux",lux);
    mqttSend("tsl2591","lux_mean",luxAgg.mean());

  mqttSend("sht40","temp_grad",tempGrad,false);
  mqttSend("sht40","rh_grad",humGrad,false);

  mqttSend("system","wifi_rssi",WiFi.RSSI());
  mqttSend("system","heap",ESP.getFreeHeap());
  mqttSend("system","heap_min",ESP.getMinFreeHeap());
  mqttSend("system","uptime",millis()/1000);
  
  // mqttSend("scd40", "co2", co2);
mqttSend("scd40", "co2_smooth", co2Smooth);
mqttSend("scd40", "co2_grad", co2Grad, false);

  tempAgg.reset();
  humAgg.reset();
  presAgg.reset();
  luxAgg.reset();
}

////////////////////////////////////////////////////////////
// WEB JSON
////////////////////////////////////////////////////////////
String jsonData(){

  auto safe = [](float v){
    return isnan(v) || isinf(v) ? 0.0f : v;
  };

  char buf[1024];

  snprintf(buf,sizeof(buf),
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

  WiFi.RSSI(),WiFi.channel(),
  ESP.getFreeHeap(),ESP.getMinFreeHeap(),
  millis()/1000,
  WiFi.localIP().toString().c_str(),
  mqtt.connected()?"true":"false"
  );

  return String(buf);
}

////////////////////////////////////////////////////////////
// WEB SERVER (ORIGINAL UI PRESERVED)
////////////////////////////////////////////////////////////

void startWeb(){
server.on("/", HTTP_GET,
[](AsyncWebServerRequest *req){

const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta name="viewport" content="width=device-width,initial-scale=1">

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>

body{
  font-family:system-ui;
  background:#0e0e0e;
  color:#eee;
  margin:16px;
}

h1{
  margin-bottom:6px;
  font-size:20px;
}

.grid{
  display:grid;
  grid-template-columns:repeat(auto-fit,minmax(200px,1fr));
  gap:8px;
}

.card{
  background:#1a1a1a;
  padding:8px;
  border-radius:8px;
}

canvas{
  width:100%;
}

.stat{
  font-size:16px;
  margin-bottom:4px;
}

.sys{
  margin-top:8px;
  font-size:13px;
  line-height:1.4;
}

</style>

</head>

<body>

<h1>ESP32 Kitchen Lab</h1>

<div class="grid">

<div class="card">
<div class="stat" id="temp_val">-- °C</div>
<canvas id="tempChart" height="180"></canvas>
</div>

<div class="card">
<div class="stat" id="hum_val">-- %</div>
<canvas id="humChart" height="180"></canvas>
</div>

<div class="card">
<div class="stat" id="pres_val">-- hPa</div>
<canvas id="presChart" height="180"></canvas>
</div>

<div class="card">
<div class="stat" id="lux_val">-- lx</div>
<canvas id="luxChart" height="180"></canvas>
</div>

<div class="card">
<div class="stat" id="co2_val">-- ppm</div>
<canvas id="co2Chart" height="180"></canvas>
</div>

<div class="card">
<div class="stat">Temp grad: <span id="tgrad_val">--</span></div>
<canvas id="tgradChart" height="180"></canvas>
</div>

<div class="card">
<div class="stat">Hum grad: <span id="hgrad_val">--</span></div>
<canvas id="hgradChart" height="180"></canvas>
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

const tgradEl=document.getElementById("tgrad_val")
const hgradEl=document.getElementById("hgrad_val")
const sysEl=document.getElementById("sys")

const MAX_POINTS=180

function chart(id,color){
return new Chart(
document.getElementById(id),
{
type:"line",
data:{
labels:[],
datasets:[{
data:[],
borderColor:color,
borderWidth:1.5,
pointRadius:0,
tension:0.15
}]
},
options:{
responsive:false,
animation:false,
plugins:{legend:{display:false}},
scales:{
x:{display:false},
y:{display:true}
}
}
})
}

const tempChart=chart("tempChart","#ff5c5c")
const humChart=chart("humChart","#5cc8ff")
const presChart=chart("presChart","#ffc857")
const luxChart=chart("luxChart","#9cff57")
const co2Chart=chart("co2Chart","#aaaaaa")
const tgradChart=chart("tgradChart","#ff9966")
const hgradChart=chart("hgradChart","#66ccff")

function push(chart,v){

const d=chart.data.datasets[0].data
const l=chart.data.labels

d.push(v)
l.push("")

if(d.length>MAX_POINTS){
d.shift()
l.shift()
}

chart.update("none")
}

async function update(){

const r=await fetch("/data")
const j=await r.json()

tempEl.innerText=j.temp.toFixed(2)+" C"
humEl.innerText=j.hum.toFixed(2)+" %"
presEl.innerText=j.pres.toFixed(1)+" hPa"
luxEl.innerText=j.lux.toFixed(0)+" lx"
co2El.innerText=j.co2.toFixed(0)+" ppm"

tgradEl.innerText=j.temp_grad.toFixed(3)
hgradEl.innerText=j.hum_grad.toFixed(3)

push(tempChart,j.temp)
push(humChart,j.hum)
push(presChart,j.pres)
push(luxChart,j.lux)
push(co2Chart,j.co2)

push(tgradChart,j.temp_grad)
push(hgradChart,j.hum_grad)

sysEl.innerHTML =
"WiFi RSSI: "+j.wifi_rssi+" dBm<br>"+
"Heap: "+j.heap+" B<br>"+
"Min heap: "+j.heap_min+" B<br>"+
"Uptime: "+j.uptime+" s<br>"+
"MQTT: "+j.mqtt

}

setInterval(update,5000)

</script>

</body>
</html>

)rawliteral";

req->send_P(200,"text/html",page);
});

server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req){
  req->send(200, "application/json", jsonData());
});

  server.begin();
  Serial.println("[WEB] started");
}

////////////////////////////////////////////////////////////
// OTA
////////////////////////////////////////////////////////////

void setupOTA(){
  ArduinoOTA.setHostname("esp32-lab");
  ArduinoOTA.begin();
}

////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////

void setup(){

  Serial.begin(115200);
  Serial.setDebugOutput(true);

setupWDT();
  Serial.println("\n===== BOOT =====");
  Serial.printf("Reset reason: %d\n", esp_reset_reason());
  Wire.setClock(100000); //reduce I2C speed
  Wire.begin(I2C_SDA,I2C_SCL);
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
  
  mqtt.setServer(MQTT_HOST,MQTT_PORT);

  printSystemInfo();

  startWeb();
  setupOTA();

}

////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////


void loop() {
  esp_task_wdt_reset();   
  wifiWatchdog();

  connectWiFi();
  connectMQTT();

  mqtt.loop();
  ArduinoOTA.handle();

  uint32_t now = millis();

  // --- SENSOR UPDATE (slow) ---
 while (millis() - lastSample >= SAMPLE_INTERVAL) {
    lastSample += SAMPLE_INTERVAL;
    readSensors();
}

  // --- AGG ---
  if (now - lastAgg > AGG_INTERVAL) {
    lastAgg = now;
    publishAgg();
  }

  // --- LED RENDER (fast, every loop) ---
  updateLED();   // <- key change

  ets_delay_us(1000);  // 1 ms microsleep to lower load
}
