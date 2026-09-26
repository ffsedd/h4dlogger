#pragma once
#include <Arduino.h>

// am312 / ld1020 status + aggregate structs live in state.h since
// leds.cpp and mqtt.cpp also need to read them.

// Configures the PIR pins and attaches a CHANGE interrupt to each
// (Phase 1): motion is an asynchronous hardware edge, so we no longer
// poll it on a fixed timer — polling on SAMPLE_INTERVAL (2 s) could
// miss a pulse shorter than that entirely and always reported motion
// up to 2 s late.
void setup_LD1020();
void setup_AM312();

// Drains the ISR-captured pin state and folds the elapsed time into the
// duty-cycle aggregators. Called once per loop() iteration (NOT gated by
// SAMPLE_INTERVAL) from indooraq.ino, so motion timing/duty-cycle
// resolution tracks the loop cadence rather than the sensor sample
// cadence.
void read_motion_sensors();
