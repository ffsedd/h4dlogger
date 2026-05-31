#include "geiger_isr.h"
#include <Arduino.h>

volatile uint32_t GeigerISR::isrCount_ = 0;
uint32_t GeigerISR::total_ = 0;
int GeigerISR::gpio_ = -1;

/* =========================
   DEBUG
========================= */
static uint32_t lastDebugMs = 0;
static uint32_t lastIsrSnapshot = 0;

GeigerISR::GeigerISR(int gpio)
{
    gpio_ = gpio;
}

bool GeigerISR::begin()
{
    Serial.println("[GeigerISR] begin()");

    pinMode(gpio_, INPUT_PULLUP);

    Serial.printf("[GeigerISR] GPIO=%d configured INPUT_PULLUP\n", gpio_);
    Serial.printf("[GeigerISR] initial pin state=%d\n", digitalRead(gpio_));

    int irq = digitalPinToInterrupt(gpio_);
    if (irq == NOT_AN_INTERRUPT)
    {
        Serial.println("[GeigerISR] ERROR: invalid interrupt pin!");
        return false;
    }
    attachInterrupt(
        irq,
        GeigerISR::isrHandler,
        FALLING);

    isrCount_ = 0;
    total_ = 0;

    lastDebugMs = millis();
    lastIsrSnapshot = 0;

    Serial.println("[GeigerISR] ready");
    return true;
}

void IRAM_ATTR GeigerISR::isrHandler()
{
    isrCount_++;
}

uint32_t GeigerISR::readAndReset()
{
    noInterrupts();

    uint32_t count = isrCount_;
    isrCount_ = 0;

    interrupts();

    total_ += count;

    // /* =========================
    //    DEBUG HEARTBEAT (SAFE OUTSIDE ISR)
    // ========================= */
    // uint32_t now = millis();
    // if (now - lastDebugMs >= 2000)
    // {
    //     lastDebugMs = now;

    //     uint32_t delta = count - lastIsrSnapshot;
    //     lastIsrSnapshot = count;

    //     Serial.printf(
    //         "[GeigerISR] IRQ snapshot: delta=%lu total=%lu pin=%d\n",
    //         (unsigned long)delta,
    //         (unsigned long)total_,
    //         digitalRead(gpio_));
    // }

    return count;
}

uint32_t GeigerISR::totalCount() const
{
    return total_;
}