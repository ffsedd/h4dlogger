#include <WiFi.h>
#include <Wire.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <PubSubClient.h>
#include <ArduinoOTA.h>

#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_TSL2591.h>

#include <esp_task_wdt.h>
#include <time.h>
#include "wifi_secrets.h"

////////////////////////////////////////////////////////////
// CONFIG
////////////////////////////////////////////////////////////



#define MQTT_HOST "10.11.12.1"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "esp32_kitchen_lab"

#define SAMPLE_INTERVAL 2000
#define AGG_INTERVAL 300000

#define I2C_SDA 21
#define I2C_SCL 22

#define WDT_TIMEOUT 30

#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

////////////////////////////////////////////////////////////
// MQTT TOPICS
////////////////////////////////////////////////////////////

#define TOPIC_STATUS "kitchen/lab/status"

#define TOPIC_TEMP_MEAN "kitchen_lab/temp_5min_mean"
#define TOPIC_TEMP_MIN  "kitchen/lab/temp_5min_min"
#define TOPIC_TEMP_MAX  "kitchen/lab/temp_5min_max"

#define TOPIC_HUM_MEAN  "kitchen/lab/hum_5min_mean"
#define TOPIC_HUM_MIN   "kitchen/lab/hum_5min_min"
#define TOPIC_HUM_MAX   "kitchen/lab/hum_5min_max"

#define TOPIC_PRES_MEAN "kitchen/lab/pres_5min_mean"
#define TOPIC_LUX_MEAN  "kitchen/lab/lux_5min_mean"

#define TOPIC_TEMP_GRAD "kitchen/lab/temp_grad"
#define TOPIC_HUM_GRAD  "kitchen/lab/hum_grad"

////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////





AsyncWebServer server(80);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Adafruit_SHT4x sht4;
Adafruit_BMP280 bmp;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

bool hasSHT=false;
bool hasBMP=false;
bool hasTSL=false;

float temp=0;
float hum=0;
float pres=0;
float lux=0;

uint32_t lastSample=0;
uint32_t lastAgg=0;
uint32_t lastTry = 0;

////////////////////////////////////////////////////////////
// SMOOTHING + GRADIENT
////////////////////////////////////////////////////////////

#define EWMA_ALPHA 0.12   // ≈30 s smoothing for 2 s sampling

float tempSmooth = NAN;
float humSmooth  = NAN;

float tempPrev = NAN;
float humPrev  = NAN;

float tempGrad = 0;
float humGrad  = 0;

////////////////////////////////////////////////////////////
// AGGREGATION
////////////////////////////////////////////////////////////

struct AggMean{

  float sum=0;
  uint32_t count=0;

  void add(float v){
    sum+=v;
    count++;
  }

  float mean() const{
    if(count==0) return NAN;
    return sum/count;
  }

  void reset(){
    sum=0;
    count=0;
  }
};

struct AggMinMax{

  float sum=0;
  float min=100000;
  float max=-100000;
  uint32_t count=0;

  void add(float v){

    sum+=v;
    count++;

    if(v<min) min=v;
    if(v>max) max=v;
  }

  float mean() const{
    if(count==0) return NAN;
    return sum/count;
  }

  void reset(){

    sum=0;
    min=100000;
    max=-100000;
    count=0;
  }
};

AggMinMax tempAgg;
AggMinMax humAgg;

AggMean presAgg;
AggMean luxAgg;

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
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);

  uint8_t tries=0;

  while(WiFi.status()!=WL_CONNECTED && tries<30){
    delay(1000);
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
Serial.print("NTP syncing");
  while(now<1000000000){
     Serial.print(".");
    delay(1000);
    now=time(nullptr);
  }
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

  if(mqtt.connect(
        MQTT_CLIENT_ID,
        TOPIC_STATUS,
        1,
        true,
        "offline")){

    mqtt.publish(TOPIC_STATUS,"online",true);
  }
}

////////////////////////////////////////////////////////////
// SENSOR DETECT
////////////////////////////////////////////////////////////

bool detectI2C(uint8_t addr){

  Wire.beginTransmission(addr);
  return Wire.endTransmission()==0;
}

void detectSensors(){
Serial.print("Detecting sensors...");
  hasSHT=detectI2C(0x44);
  hasBMP=detectI2C(0x76)||detectI2C(0x77);
  hasTSL=detectI2C(0x29);

  if(hasSHT)
    Serial.print(" SHT4x");
    sht4.begin(&Wire);

if(hasBMP){
  Serial.print(" BMP280");
  if(!bmp.begin(0x76))
    if(!bmp.begin(0x77))
      hasBMP=false;
}

  if(hasTSL){
Serial.print(" TSL2591");
    tsl.begin();
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
  }
}

////////////////////////////////////////////////////////////
// SENSOR READ
////////////////////////////////////////////////////////////

void readSensors(){

  if(hasSHT){

    sensors_event_t h,t;
if(sht4.getEvent(&h,&t)){

  temp = t.temperature;
  hum  = h.relative_humidity;

  tempAgg.add(temp);
  humAgg.add(hum);

  ////////////////////////////////////////////////////////////
  // EWMA smoothing
  ////////////////////////////////////////////////////////////

  if(isnan(tempSmooth)){
    tempSmooth = temp;
    humSmooth  = hum;
  }else{
    tempSmooth = EWMA_ALPHA*temp + (1.0-EWMA_ALPHA)*tempSmooth;
    humSmooth  = EWMA_ALPHA*hum  + (1.0-EWMA_ALPHA)*humSmooth;
  }

  ////////////////////////////////////////////////////////////
  // gradient from smoothed signal
  ////////////////////////////////////////////////////////////

  if(!isnan(tempPrev))
    tempGrad = (tempSmooth - tempPrev) * 30.0;   // °C per minute

  if(!isnan(humPrev))
    humGrad  = (humSmooth - humPrev)  * 30.0;    // %RH per minute

  tempPrev = tempSmooth;
  humPrev  = humSmooth;
}
  }

  if(hasBMP){

    pres=bmp.readPressure()/100.0;
    presAgg.add(pres);
  }

  if(hasTSL){

    sensors_event_t light;
    tsl.getEvent(&light);

    lux=light.light;

    if(lux>0 && lux<200000)
      luxAgg.add(lux);
  }
}

////////////////////////////////////////////////////////////
// PUBLISH AGGREGATES
////////////////////////////////////////////////////////////

void publishAgg(){

  if(!mqtt.connected())
    return;

  char buf[32];

float v=tempAgg.mean();
if(!isnan(v)){
  snprintf(buf,sizeof(buf),"%.2f",v);
  mqtt.publish(TOPIC_TEMP_MEAN,buf,true);
}

  snprintf(buf,sizeof(buf),"%.2f",tempAgg.min);
  mqtt.publish(TOPIC_TEMP_MIN,buf,true);

  snprintf(buf,sizeof(buf),"%.2f",tempAgg.max);
  mqtt.publish(TOPIC_TEMP_MAX,buf,true);

  snprintf(buf,sizeof(buf),"%.2f",humAgg.mean());
  mqtt.publish(TOPIC_HUM_MEAN,buf,true);

  snprintf(buf,sizeof(buf),"%.2f",humAgg.min);
  mqtt.publish(TOPIC_HUM_MIN,buf,true);

  snprintf(buf,sizeof(buf),"%.2f",humAgg.max);
  mqtt.publish(TOPIC_HUM_MAX,buf,true);

  snprintf(buf,sizeof(buf),"%.2f",presAgg.mean());
  mqtt.publish(TOPIC_PRES_MEAN,buf,true);

  snprintf(buf,sizeof(buf),"%.2f",luxAgg.mean());
  mqtt.publish(TOPIC_LUX_MEAN,buf,true);

  snprintf(buf,sizeof(buf),"%.3f",tempGrad);
  mqtt.publish(TOPIC_TEMP_GRAD,buf,false);

  snprintf(buf,sizeof(buf),"%.3f",humGrad);
  mqtt.publish(TOPIC_HUM_GRAD,buf,false);

  tempAgg.reset();
  humAgg.reset();
  presAgg.reset();
  luxAgg.reset();
}

////////////////////////////////////////////////////////////
// WEB JSON
////////////////////////////////////////////////////////////

String jsonData(){



char buf[320];

snprintf(buf, sizeof(buf),
"{"
"\"temp\":%.2f,"
"\"hum\":%.2f,"
"\"pres\":%.2f,"
"\"lux\":%.2f,"

"\"temp_smooth\":%.2f,"
"\"hum_smooth\":%.2f,"

"\"temp_grad\":%.3f,"
"\"hum_grad\":%.3f,"

"\"wifi_rssi\":%d,"
"\"wifi_ch\":%d,"

"\"heap\":%u,"
"\"heap_min\":%u,"

"\"uptime\":%lu,"

"\"ip\":\"%s\","
"\"mqtt\":%s"
"}",
temp,
hum,
pres,
lux,

tempSmooth,
humSmooth,

tempGrad,
humGrad,

WiFi.RSSI(),
WiFi.channel(),

ESP.getFreeHeap(),
ESP.getMinFreeHeap(),

millis()/1000,

WiFi.localIP().toString().c_str(),
mqtt.connected() ? "true" : "false"
);

  return String(buf);
}

////////////////////////////////////////////////////////////
// WEB SERVER
////////////////////////////////////////////////////////////
void startWeb(){

server.on("/data",HTTP_GET,
[](AsyncWebServerRequest *req){

  req->send(200,"application/json",jsonData());

});

server.on("/",HTTP_GET,
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
<div class="stat" id="lux_val">-- lux</div>
<canvas id="luxChart" height="180"></canvas>
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

tgradEl.innerText=j.temp_grad.toFixed(3)
hgradEl.innerText=j.hum_grad.toFixed(3)

push(tempChart,j.temp)
push(humChart,j.hum)
push(presChart,j.pres)
push(luxChart,j.lux)
push(tgradChart,j.temp_grad)
push(hgradChart,j.hum_grad)

sysEl.innerHTML =
"WiFi RSSI: "+j.wifi_rssi+" dBm<br>"+
"Heap: "+j.heap+" B<br>"+
"Min heap: "+j.heap_min+" B<br>"+
"Uptime: "+j.uptime+" s<br>"+
"MQTT: "+j.mqtt

}

setInterval(update,2000)

</script>

</body>
</html>
)rawliteral";

req->send_P(200,"text/html",page);

});

server.begin();

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

  Wire.begin(I2C_SDA,I2C_SCL);

  connectWiFi();

  setupTime();

  mqtt.setServer(MQTT_HOST,MQTT_PORT);

  detectSensors();

  startWeb();

  setupOTA();

  esp_task_wdt_config_t wdt_config={
      .timeout_ms=WDT_TIMEOUT*1000,
      .idle_core_mask=0,
      .trigger_panic=true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
}

////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////

void loop(){

  esp_task_wdt_reset();

  wifiWatchdog();

  connectWiFi();
  connectMQTT();

  mqtt.loop();

  ArduinoOTA.handle();

  uint32_t now=millis();

  if(now-lastSample>SAMPLE_INTERVAL){

    lastSample=now;

    readSensors();
  }

  if(now-lastAgg>AGG_INTERVAL){

    lastAgg=now;

    publishAgg();
  }
}