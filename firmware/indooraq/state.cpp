#include "state.h"

// ---------------------------------------------------------
// Hardware handles
// ---------------------------------------------------------
AsyncWebServer server(80);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Adafruit_SHT4x sht4;
Adafruit_BMP280 bmp;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
SCD4x scd4;

// ---------------------------------------------------------
// Sensor presence / init status
// ---------------------------------------------------------
SensorStatus shtStat, bmpStat, tslStat, scdStat;
bool i2cDevices[128] = {false};

// ---------------------------------------------------------
// Motion sensors
// ---------------------------------------------------------
am312Status am312;
AM312Agg am312Agg;
LD1020Status ld1020;
LD1020Agg ld1020Agg;

// ---------------------------------------------------------
// Live + smoothed sensor readings
// ---------------------------------------------------------
float temp = NAN, hum = NAN, pres = NAN, lux = NAN;
float tempSmooth = NAN, humSmooth = NAN, luxSmooth = NAN;
float tempPrev = NAN, humPrev = NAN, co2Prev = NAN;
float tempGrad = 0, humGrad = 0, co2Grad = 0;
float co2 = NAN;
float co2Smooth = NAN;
float cpuTemp = NAN;

// ---------------------------------------------------------
// Aggregation helpers
// ---------------------------------------------------------
AggMinMax tempAgg, humAgg;
AggMean presAgg, luxAgg, co2Agg;

// ---------------------------------------------------------
// Timing / scheduling state
// ---------------------------------------------------------
time_t aggStartTs = 0;
uint32_t lastSample = 0;
uint32_t lastAgg = 0;
uint32_t lastMqttTry = 0;

// ---------------------------------------------------------
// WiFi state
// ---------------------------------------------------------
uint32_t wifiLastAttempt = 0;
uint32_t wifiOfflineSince = 0;
bool WIFI_SLEEP = true;

// ---------------------------------------------------------
// OTA state
// ---------------------------------------------------------
volatile bool otaInProgress = false;
