#include "oled_ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32 // most 0.91" are 128x32

#define OLED_ADDR 0x3C

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void lcdInit()
{
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
    {
        Serial.println("SSD1306 init failed");
        while (true)
            delay(100);
    }
    Serial.println("SSD1306 init done");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("Geiger init...");
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(0xFF);
    display.display();
    delay(1000);
}

void lcdUpdate(float cps, float avg)
{
    display.clearDisplay();

    display.setCursor(0, 0);
    display.printf("CPS: %.2f\n", cps);

    display.printf("AVG: %.2f\n", avg);

    display.display();
}