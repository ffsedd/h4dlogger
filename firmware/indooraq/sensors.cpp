#include <Wire.h>
#include "sensors.h"
#include "config.h"
#include "state.h"
#include "motion.h"

////////////////////////////////////////////////////////////
// FILTERS (local to this module — nothing else needs the
// EMA instances directly, only the smoothed values in state.h)
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

static EMA tempLPF(10.0f), humLPF(10.0f), luxLPF(5.0f), co2LPF(5.0f);
static EMA tempGradLPF(120.0f), humGradLPF(120.0f), co2GradLPF(180.0f);

static float tempHist[3] = {NAN, NAN, NAN};
static float humHist[3] = {NAN, NAN, NAN};
static float co2Hist[3] = {NAN, NAN, NAN};

static inline void push3(float *h, float v)
{
  h[0] = h[1];
  h[1] = h[2];
  h[2] = v;
}

////////////////////////////////////////////////////////////
// I2C DISCOVERY
////////////////////////////////////////////////////////////

bool is_known_i2c_addr(uint8_t addr)
{
  return addr == 0x44 || addr == 0x76 || addr == 0x77 || addr == 0x29 || addr == 0x62;
}

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
      Serial.printf("[I2C] 0x%02X  %s\n", addr, is_known_i2c_addr(addr) ? "KNOWN" : "UNKNOWN \u26A0");
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

////////////////////////////////////////////////////////////
// FAST SENSORS (SHT4x, BMP280, TSL2591, + motion)
////////////////////////////////////////////////////////////

void read_fast_sensors()
{
  const float dt = SAMPLE_INTERVAL / 1000.0f;

  // Motion is no longer read here -- it's interrupt-driven (Phase 1) and
  // drained once per loop() iteration by read_motion_sensors(), called
  // directly from indooraq.ino so its timing isn't tied to SAMPLE_INTERVAL.

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

////////////////////////////////////////////////////////////
// SCD4x CO2 (its own slower cadence)
////////////////////////////////////////////////////////////

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

  cpuTemp = temperatureRead();
}
