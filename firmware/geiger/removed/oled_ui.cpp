#include "oled_ui.h"

static U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C oled(
    U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ---------------- chart buffer ----------------
static constexpr uint8_t CHART_W = 64;
static constexpr uint8_t CHART_H = 14;

static float cpsChart[CHART_W];
static uint8_t chartIdx = 0;
static bool chartFull = false;

void chartPush(float v)
{
    cpsChart[chartIdx] = v;

    chartIdx++;
    if (chartIdx >= CHART_W)
    {
        chartIdx = 0;
        chartFull = true;
    }
}

void drawChart()
{
    uint8_t n = chartFull ? CHART_W : chartIdx;
    if (n < 2)
        return;

    float maxv = 1.0f;

    for (uint8_t i = 0; i < n; i++)
        if (cpsChart[i] > maxv)
            maxv = cpsChart[i];

    const uint8_t baseY = 31;
    const uint8_t topY = 16;
    const uint8_t h = baseY - topY;

    uint8_t start = chartFull ? chartIdx : 0;

    for (uint8_t x = 0; x < n && x < 128; x++)
    {
        uint8_t i = (start + x) % CHART_W;

        float v = cpsChart[i];
        uint8_t bar = (uint8_t)((v / maxv) * h);

        if (bar > h)
            bar = h;

        oled.drawVLine(x, baseY, -bar); // cleaner upward drawing
    }
}

void lcdInit()
{
    oled.begin();
    oled.setContrast(255);

    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(0, 10, "Geiger boot");
    oled.sendBuffer();
}

void lcdUpdate(float cps)
{
    static float ema = 0;

    ema = 0.8f * ema + 0.2f * cps;
    chartPush(ema);

    char buf[32];

    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tr);
    snprintf(buf, sizeof(buf), "%.1f cps", cps);
    oled.drawStr(0, 10, buf);

    drawChart();

    oled.sendBuffer();
}
