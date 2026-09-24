#pragma once
#include <Arduino.h>

bool is_known_i2c_addr(uint8_t addr);
void scan_i2c_devices();
void init_i2c_sensors();

// Reads SHT4x / BMP280 / TSL2591 + motion pins, applies EMA smoothing
// and feeds the 5-minute aggregators. Called every SAMPLE_INTERVAL.
void read_fast_sensors();

// Reads SCD4x CO2 on its own slower cadence (SAMPLE_INTERVAL_SCD40).
void read_SCD40();

// Convenience wrapper: read_fast_sensors() + read_SCD40() + cpu temp.
void read_sensors();
