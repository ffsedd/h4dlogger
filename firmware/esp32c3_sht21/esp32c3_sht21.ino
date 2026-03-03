#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <SHTSensor.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_sleep.h"
#include "wifi_secrets.h"

// ================= Pin Config =================
constexpr uint8_t PIN_I2C_SDA = 5;
constexpr uint8_t PIN_I2C_SCL = 6;

// ================= WiFi & MQTT Config =================
constexpr const char* MQTT_HOST = "10.11.12.1";
constexpr uint16_t    MQTT_PORT = 1883;
constexpr const char* CLIENT_ID = "esp32c3_bedroom_ceiling";

// Updated MQTT topics to reflect SHT21
constexpr const char* TOPIC_BOOT  = "bedroom_ceiling/sht21/boot";
constexpr const char* TOPIC_TEMP  = "bedroom_ceiling/sht21/temp";
constexpr const char* TOPIC_HUM   = "bedroom_ceiling/sht21/rh";

// Sampling configuration
constexpr uint32_t SAMPLE_INTERVAL_S = 5;
constexpr uint32_t UPLOAD_INTERVAL_S = 60;
constexpr uint32_t SAMPLES_PER_BATCH = UPLOAD_INTERVAL_S / SAMPLE_INTERVAL_S;

// ================= Globals =================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);
// Use SHT2X for SHT21/SHT25
SHTSensor sht(SHTSensor::SHT2X);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

struct Sample { float t; float h; };

RTC_DATA_ATTR Sample buffer[SAMPLES_PER_BATCH];
RTC_DATA_ATTR uint32_t sampleIndex = 0;
RTC_DATA_ATTR uint32_t bootCount   = 0;

// ================= Utility Functions =================
const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        default: return "OTHER";
    }
}

// ================= WiFi / MQTT =================
void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);
}

void connectMQTT() {
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);
    uint32_t t0 = millis();
    while (!mqtt.connected() && millis() - t0 < 10000) {
        mqtt.connect(CLIENT_ID);
        delay(200);
    }
}

void disconnectWiFi() {
    if (mqtt.connected()) mqtt.disconnect();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

// ================= MQTT Publishing =================
void publishBootEvent() {
    char payload[64];
    snprintf(payload, sizeof(payload),
             "{\"boot\":%lu,\"reason\":\"%s\"}",
             bootCount, resetReasonStr(esp_reset_reason()));
    mqtt.publish(TOPIC_BOOT, payload, true);
}

void publishBatch() {
    connectWiFi();
    connectMQTT();

    if (!mqtt.connected()) {
        Serial.println("MQTT connection failed, skipping batch");
        return;
    }

    publishBootEvent();

    char payload[32];
    for (uint32_t i = 0; i < sampleIndex; ++i) {
        snprintf(payload, sizeof(payload), "%.1f", buffer[i].t);
        mqtt.publish(TOPIC_TEMP, payload);

        snprintf(payload, sizeof(payload), "%.1f", buffer[i].h);
        mqtt.publish(TOPIC_HUM, payload);
    }

    disconnectWiFi();
    sampleIndex = 0;
}

// ================= Setup =================
void setup() {
    delay(1000);
    Serial.begin(115200);

    bootCount++;
    Serial.printf("=== BOOT #%lu === Reason: %s\n", bootCount, resetReasonStr(esp_reset_reason()));

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    oled.begin();

    if (!sht.init()) {
        Serial.println("SHT21 init failed");
    }
}

// ================= Main Loop =================
void loop() {
    if (sht.readSample() && sampleIndex < SAMPLES_PER_BATCH) {
        buffer[sampleIndex].t = sht.getTemperature();
        buffer[sampleIndex].h = sht.getHumidity();
        sampleIndex++;
    }

    if (sampleIndex >= SAMPLES_PER_BATCH) {
        publishBatch();
    }

    // Sleep until next sample
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(SAMPLE_INTERVAL_S) * 1000000ULL);
    esp_deep_sleep_start();
}
