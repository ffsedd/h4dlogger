#pragma once
#include <Arduino.h>

void setupLED();
void led_set(uint8_t r, uint8_t y, uint8_t g);
void led_off();
void led_flash(uint8_t r, uint8_t y, uint8_t g, uint16_t on_ms, uint16_t off_ms);

// Phase 3: kicks off the NTP-success flash pattern and returns immediately.
// The pattern is a non-blocking state machine advanced a step at a time
// from updateLED() (driven by real elapsed time via millis(), so it's
// correct regardless of how often/when updateLED() gets called) -- unlike
// the old led_ntp_success_sequence(), which blocked for ~1.5 s of delay()
// calls. It's called during setup(), before loop() exists yet, so nothing
// else was actually stalled by the old version; this is a correctness/
// consistency cleanup so no code path in this project uses delay()-driven
// LED sequencing.
void led_start_ntp_success_sequence();

////////////////////////////////////////////////////////////
// ONBOARD LED — 4-bit framed error code (GPIO8, single color, active LOW)
////////////////////////////////////////////////////////////

void setupOnboardLED();

// Non-blocking, called every loop() iteration alongside updateLED().
// Repeatedly blinks a 4-bit code: bit 0 and bit 3 are always 1
// (start/stop framing, so the pattern is recognizable even if you start
// watching mid-blink), the middle two bits encode system state, checked
// in this priority order:
//   1 0 0 1   OK
//   1 1 1 1   WiFi not connected
//   1 0 1 1   MQTT not connected
//   1 1 0 1   other error (see report_other_error() below)
void updateOnboardLED();

// Raises/clears the catch-all "other error" condition (shown as 1101
// when WiFi+MQTT are otherwise fine). Already auto-set when a detected
// I2C sensor fails to initialize; call this from anywhere else you want
// to flag a condition that doesn't have its own dedicated code.
void report_other_error(bool active);

// Non-blocking, called every loop() iteration.
// Reflects CO2 level as a green->yellow->red gradient, gated by
// recent motion, with blink patterns for idle/warning/critical.
void updateLED();
