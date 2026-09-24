#include "oled_ui.h"
#include <U8g2lib.h>

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, 6, 5);
uint8_t x_offset = 40;
uint8_t y_offset = 30;

void lcdInit()
{
    oled.begin();
    oled.setContrast(255);
    oled.clearBuffer();
    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(0, 10, "Geiger boot");
    oled.sendBuffer();
}

void lcdUpdate(float cps)
{
    char b1[32], b2[32];

    oled.clearBuffer();
    // oled.setFont(u8g2_font_6x10_tr);
    // oled.setFont(u8g2_font_8x13_tr);
    // oled.setFont(u8g2_font_10x20_tr);
    oled.setFont(u8g2_font_courB14_tr);

    snprintf(b1, sizeof(b1), "%.1f", cps);

    oled.drawStr(x_offset, y_offset, b1);

    oled.sendBuffer();
}