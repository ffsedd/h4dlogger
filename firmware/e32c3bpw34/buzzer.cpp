#include "buzzer.h"

#include <Arduino.h>

static constexpr int BUZZER_PIN = 3;

static constexpr float SIGNAL_MIN = 0.0f;
static constexpr float SIGNAL_MAX = 1000.0f;

static constexpr uint32_t BEEP_MIN_MS = 10;
static constexpr uint32_t BEEP_MAX_MS = 20000;

static uint32_t lastBeep = 0;

static uint32_t beepInterval(float signal)
{
    const float x = constrain(signal, SIGNAL_MIN, SIGNAL_MAX);
    const float f = (x - SIGNAL_MIN) / (SIGNAL_MAX - SIGNAL_MIN);

    return BEEP_MAX_MS - f * (BEEP_MAX_MS - BEEP_MIN_MS);
}

void updateBuzzer(float signal)
{
    const uint32_t now = millis();
    const uint32_t interval = beepInterval(signal);

    if (now - lastBeep >= interval)
    {
        lastBeep = now;
        tone(BUZZER_PIN, 2500, 10);
    }
}