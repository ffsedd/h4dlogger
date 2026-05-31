#include "lcd_ui.h"
#include <WiFi.h>

static hd44780_I2Cexp lcd;

void lcdInit()
{
    if(lcd.begin(16,2)) return;

    //~ lcd.backlight();
    lcd.clear();
    lcd.print("init...");
}

void lcdUpdate(float cps,float mean)
{
    static uint32_t last=0;
    if(millis()-last<1000) return;
    last=millis();

    char l1[17];
    char l2[17];

    snprintf(l1,sizeof(l1),
        "CPS:%5.2f %4.0f",mean,cps);

    if(WiFi.status()==WL_CONNECTED)
    {
        auto ip=WiFi.localIP();
        snprintf(l2,sizeof(l2),
            "%d.%d.%d.%d",
            ip[0],ip[1],ip[2],ip[3]);
    }
    else strcpy(l2,"offline");

    lcd.setCursor(0,0);
    lcd.print(l1);

    lcd.setCursor(0,1);
    lcd.print(l2);
}
