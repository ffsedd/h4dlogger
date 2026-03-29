// esp32c3_wifi_ota.ino
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "wifi_secrets.h"


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

void setup() {
  // Initialize Serial for debug (optional, can be removed if unstable)
  Serial.begin(115200);
  delay(100);

  // LED as status
  pinMode(LED_BUILTIN, OUTPUT);

  // Connect to Wi-Fi
  connectWiFi();
  // Setup OTA
  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA Start");
    })
    .onEnd([]() {
      Serial.println("\nOTA End");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\n", (progress * 100) / total);
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]\n", error);
    });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void loop() {
  // Blink LED
  static unsigned long last = 0;
  if (millis() - last > 500) {
    last = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // Handle OTA updates
  ArduinoOTA.handle();

  delay(500);
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
}
