#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <ESPAsyncWebServer.h>
#include "wifi_secrets.h"

#define SDA_PIN 7
#define SCL_PIN 6

Adafruit_SHT4x sht4 = Adafruit_SHT4x();
AsyncWebServer server(80);

// Sensor values
float temp = NAN;
float hum = NAN;

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!sht4.begin(&Wire)) {
    // SHT40 init failed
    temp = NAN;
    hum = NAN;
  }

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // Setup webserver
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='1'><title>SHT40</title></head><body>";
    page += "<h1>SHT40 Sensor</h1>";
    if (!isnan(temp) && !isnan(hum)) {
      page += "<p>Temperature: " + String(temp,2) + " C</p>";
      page += "<p>Humidity: " + String(hum,2) + " %</p>";
    } else {
      page += "<p>Sensor not initialized or failed!</p>";
    }
    page += "</body></html>";
    request->send(200, "text/html", page);
  });
  server.begin();
}

void loop() {
  // Read sensor
  sensors_event_t h, t;
  if (sht4.getEvent(&h, &t)) {
    temp = t.temperature;
    hum = h.relative_humidity;
  } else {
    temp = NAN;
    hum = NAN;
  }

  delay(1000); // read every second
}
