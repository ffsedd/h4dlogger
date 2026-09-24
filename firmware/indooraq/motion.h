#pragma once
#include <Arduino.h>

// am312 / ld1020 status + aggregate structs live in state.h since
// leds.cpp and mqtt.cpp also need to read them.

void setup_LD1020();
void setup_AM312();

// Polls both PIR pins, updates am312/ld1020 status + aggregators.
// Called once per SAMPLE_INTERVAL from sensors.cpp's read_fast_sensors().
void read_motion_sensors();
