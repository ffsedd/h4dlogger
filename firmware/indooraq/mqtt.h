#pragma once
#include <Arduino.h>

void connect_MQTT();

// topic = DEVICE_ID/sensor/metric, payload = "timestamp,value"
void mqttSendCSV(unsigned long ts, const char *sensor, const char *metric,
                  float value, bool retained = false);

// Flushes the 5-minute aggregators to MQTT and resets them.
void publish_Agg_values();
