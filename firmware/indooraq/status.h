#pragma once
#include <Arduino.h>

void print_sysinfo();

// Builds the JSON payload served at GET /data (see web.ino).
String jsonData();
