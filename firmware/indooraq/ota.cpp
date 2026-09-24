#include <ArduinoOTA.h>
#include "ota.h"
#include "config.h"
#include "state.h"
#include "leds.h"

void setupOTA()
{
  ArduinoOTA.setHostname(DEVICE_ID);

  ArduinoOTA
      .onStart([]()
               {
        Serial.println("[OTA] Start");
        otaInProgress = true;
        led_set(255, 255, 0); // ota indicator
      })
      .onEnd([]()
             {
        Serial.println("\n[OTA] End");
        otaInProgress = false;
        led_set(0, 0, 255); // ota indicator
      })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("[OTA] %u%%\r", progress * 100 / total); })
      .onError([](ota_error_t error)
               {
        otaInProgress = false;
        Serial.printf("[OTA] Error[%u]\n", error);
      });

  ArduinoOTA.begin();
  Serial.println("[OTA] Ready");
}
