#pragma once
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>


void lcdInit();
void lcdUpdate(float cps,float mean);
