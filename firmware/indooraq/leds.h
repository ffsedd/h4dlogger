#pragma once
#include <Arduino.h>

void setupLED();
void led_set(uint8_t r, uint8_t y, uint8_t g);
void led_off();
void led_flash(uint8_t r, uint8_t y, uint8_t g, uint16_t on_ms, uint16_t off_ms);
void led_ntp_success_sequence();

// Non-blocking, called every loop() iteration.
// Reflects CO2 level as a green->yellow->red gradient, gated by
// recent motion, with blink patterns for idle/warning/critical.
void updateLED();
