// indooraq.ino
//
// Environmental sensor node (SHT4x / BMP280 / TSL2591 / SCD4x + PIR motion),
// publishing aggregated readings over MQTT and serving a live status page.
//
// Module layout:
//   config.h    - pins, intervals, build-time constants
//   state.h/.cpp- shared runtime state (hardware handles, readings, aggregators)
//   leds.h/.cpp - status LED (CO2 gradient + NTP/OTA indicators)
//   motion.h/.cpp - PIR sensors (AM312 / LD1020)
//   sensors.h/.cpp - I2C sensors, filtering, aggregation feed
//   network.h/.cpp - WiFi connect/watchdog + NTP sync
//   mqtt.h/.cpp - MQTT connect + CSV publish
//   ota.h/.cpp  - ArduinoOTA setup
//   status.h/.cpp - system info printout + /data JSON for the web UI
//   web.ino     - AsyncWebServer routes (dashboard HTML + /data)

// Kept from the original sketch even though nothing here uses them yet —
// harmless to include, and one less thing to rediscover if a future
// feature (config persistence / structured MQTT payloads) needs them.
#include <time.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "config.h"
#include "state.h"
#include "leds.h"
#include "motion.h"
#include "sensors.h"
#include "network.h"
#include "mqtt.h"
#include "ota.h"
#include "status.h"

void startWeb(); // implemented in web.ino

////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////

// Prints "[BOOT] <label>" plus milliseconds elapsed since the previous
// checkpoint, so a slow/hanging boot (WiFi scan, NTP, sensor init) shows
// up clearly in the serial log instead of just going quiet.
static uint32_t bootCheckpointTs = 0;
static void bootLog(const char *label)
{
  uint32_t now = millis();
  uint32_t delta = (bootCheckpointTs == 0) ? 0 : (now - bootCheckpointTs);
  Serial.printf("[BOOT] %-28s t=%6lu ms  (+%lu ms)\n", label, (unsigned long)now, (unsigned long)delta);
  bootCheckpointTs = now;
}

void setup()
{
  setupLED();
  led_set(64, 64, 64); // boot indicator

  setupOnboardLED(); // GPIO8 error-code blinker — see leds.cpp

  // ---------- SERIAL ----------
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(100); // let the USB/serial monitor catch up before we start printing

  Serial.println("\n===== BOOT =====");
  Serial.printf("Reset reason: %d\n", esp_reset_reason());
  bootLog("start");

  // ---------- I2C / HARDWARE FIRST ----------
  Wire.setTimeout(50);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  bootLog("i2c bus up");

  scan_i2c_devices();
  bootLog("i2c scan done");

  init_i2c_sensors();
  bootLog("sensors init done");
  Serial.printf("[BOOT]   SHT4x=%s BMP280=%s TSL2591=%s SCD4x=%s\n",
                shtStat.initialized ? "OK" : (shtStat.present ? "FAIL" : "absent"),
                bmpStat.initialized ? "OK" : (bmpStat.present ? "FAIL" : "absent"),
                tslStat.initialized ? "OK" : (tslStat.present ? "FAIL" : "absent"),
                scdStat.initialized ? "OK" : (scdStat.present ? "FAIL" : "absent"));

  setup_LD1020();
  setup_AM312();
  bootLog("motion sensors init done");

  // ---------- WIFI STACK ----------
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(WIFI_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.disconnect(true, true);

  delay(200);
  bootLog("wifi stack ready");

  Serial.println("[BOOT] scanning + connecting to best known AP...");
  connect_best_wifi();
  bootLog("wifi connect requested");

  // ---------- WAIT FOR CONNECTION ----------
  bool wifiOk = wait_for_wifi(); // IMPORTANT
  bootLog(wifiOk ? "wifi connected" : "wifi connect FAILED/timeout");

  // From here on, all reconnect scans/connects run on the background
  // wifi task (Phase 2) -- this initial connect stays synchronous on
  // purpose, since boot needs WiFi up before NTP/MQTT/web setup below.
  start_wifi_task();
  bootLog("wifi background task started");

  // ---------- STATUS ----------
  if (WiFi.isConnected())
  {
    Serial.printf("[WiFi] IP=%s RSSI=%d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
  }
  else
  {
    Serial.println("[WiFi] not connected — continuing boot anyway, watchdog will retry in loop()");
  }

  print_sysinfo();
  bootLog("sysinfo printed");

  // ---------- TIME ----------
  Serial.println("[BOOT] syncing NTP time...");
  sync_ntp_time();
  bootLog("ntp sync done");

  // ---------- NETWORK SERVICES ----------
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  bootLog("mqtt server configured");

  startWeb(); // needs IP
  bootLog("web server started");

  setupOTA(); // safest last
  bootLog("ota ready");

  setCpuFrequencyMhz(CPU_FREQ_MHZ);
  bootLog("cpu freq set");

  Serial.println("===== BOOT COMPLETE =====\n");
}

////////////////////////////////////////////////////////////
// LOOP
////////////////////////////////////////////////////////////
void loop()
{
  ArduinoOTA.handle();

  uint32_t now = millis();

  // ---------- MOTION (Phase 1: interrupt-driven, every loop tick) ----------
  read_motion_sensors();

  // ---------- WIFI WATCHDOG (Phase 2: background task) ----------
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
  updateOnboardLED();

  delay(5);
}