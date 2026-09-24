#include "mqtt.h"
#include "config.h"
#include "state.h"

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

  // Build LWT topic
  char topicStatus[64];
  snprintf(topicStatus, sizeof(topicStatus), "%s/system/status", DEVICE_ID);

  // Last Will payload (offline)
  char willPayload[64];
  snprintf(willPayload, sizeof(willPayload), "%lu,0", (unsigned long)time(nullptr));

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
    mqttSendCSV(time(nullptr), "system", "status", 1, true);
  }
  else
  {
    Serial.print("FAIL rc=");
    Serial.println(mqtt.state());
  }
}

////////////////////////////////////////////////////////////
// MQTT CSV Sender
// topic   = device/sensor/metric
// payload = timestamp,value
////////////////////////////////////////////////////////////

void mqttSendCSV(unsigned long ts, const char *sensor, const char *metric,
                  float value, bool retained)
{
  if (!mqtt.connected())
    return;

  char topic[96];
  char payload[64];

  // topic: kitchen/sht40/temp
  snprintf(topic, sizeof(topic), "%s/%s/%s", DEVICE_ID, sensor, metric);

  // payload: 1774671828,22.341
  snprintf(payload, sizeof(payload), "%lu,%.3f", ts, value);

  mqtt.publish(topic, (uint8_t *)payload, strlen(payload), retained);
}

////////////////////////////////////////////////////////////
// PUBLISH AGGREGATED DATA
////////////////////////////////////////////////////////////

void publish_Agg_values()
{
  if (!mqtt.connected())
    return;

  // Initialize aggregation window start
  if (aggStartTs == 0)
    aggStartTs = time(nullptr);

  // Correct CENTER timestamp of aggregation interval
  const time_t ts_center = aggStartTs + (AGG_INTERVAL / 2000); // ms -> s /2

  // ---------- Temperature ----------
  if (tempAgg.count)
  {
    mqttSendCSV(ts_center, "sht40", "temp_mean", tempAgg.mean());
    mqttSendCSV(ts_center, "sht40", "temp_min", tempAgg.min);
    mqttSendCSV(ts_center, "sht40", "temp_max", tempAgg.max);
  }

  // ---------- Humidity ----------
  if (humAgg.count)
  {
    mqttSendCSV(ts_center, "sht40", "rh_mean", humAgg.mean());
    mqttSendCSV(ts_center, "sht40", "rh_min", humAgg.min);
    mqttSendCSV(ts_center, "sht40", "rh_max", humAgg.max);
  }

  // ---------- Pressure ----------
  if (presAgg.count)
    mqttSendCSV(ts_center, "bmp280", "pressure_mean", presAgg.mean());

  // ---------- Light ----------
  if (luxAgg.count)
    mqttSendCSV(ts_center, "tsl2591", "lux_mean", luxAgg.mean());

  // ---------- Gradients (VALIDITY GUARDED) ----------
  const uint16_t MIN_GRAD_SAMPLES = 3;

  if (tempAgg.count > MIN_GRAD_SAMPLES)
    mqttSendCSV(ts_center, "sht40", "temp_grad", tempGrad);

  if (humAgg.count > MIN_GRAD_SAMPLES)
    mqttSendCSV(ts_center, "sht40", "rh_grad", humGrad);

  if (co2Agg.count > MIN_GRAD_SAMPLES)
    mqttSendCSV(ts_center, "scd40", "co2_grad", co2Grad);

  // ---------- System ----------
  mqttSendCSV(ts_center, "system", "wifi_rssi", WiFi.RSSI());
  mqttSendCSV(ts_center, "system", "heap", ESP.getFreeHeap());
  mqttSendCSV(ts_center, "system", "heap_min", ESP.getMinFreeHeap());
  mqttSendCSV(ts_center, "system", "uptime", millis() / 1000.0f);

  // ---------- CO2 ----------
  mqttSendCSV(ts_center, "scd40", "co2_smooth", co2Smooth);

  // ---------- LD1020 motion ----------
  if (ld1020Agg.totalCount > 0)
    mqttSendCSV(ts_center, "ld1020", "motion_fraction", ld1020Agg.fraction());

  // ---------- AM312 motion ----------
  if (am312Agg.totalCount > 0)
    mqttSendCSV(ts_center, "am312", "motion_fraction", am312Agg.fraction());

  // ---------- RESET AGGREGATORS ----------
  tempAgg.reset();
  humAgg.reset();
  presAgg.reset();
  luxAgg.reset();
  ld1020Agg.reset();
  am312Agg.reset();
  co2Agg.reset(); // IMPORTANT for gradient validity

  // ---------- START NEW AGGREGATION WINDOW ----------
  aggStartTs = time(nullptr);
  lastAgg = millis();

  // ---------- SERIAL PRINT ALL ----------
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
