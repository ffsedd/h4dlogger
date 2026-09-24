#pragma once
#include <Arduino.h>
#include <ArduinoOTA.h> // pulled in here so esp32c3sm.ino can call ArduinoOTA.handle()

void setupOTA();
