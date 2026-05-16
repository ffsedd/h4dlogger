#include <Wire.h>

bool i2cDevices[128] = {false};

/* =====================================================
   I2C STATE
===================================================== */

uint8_t i2cCount = 0;
uint8_t lastI2CAddr = 0;

/* =====================================================
   DEVICE MAP
===================================================== */

struct I2CDeviceInfo
{
    uint8_t addr;
    const char *name;
};

static const I2CDeviceInfo knownDevices[] =
    {
        {0x20, "MCP23017"},
        {0x21, "MCP23017"},
        {0x27, "PCF8574 LCD"},
        {0x3C, "SSD1306 OLED"},
        {0x29, "TSL2591"},
        {0x40, "SHT4x"},
        {0x44, "SHT4x"},
        {0x48, "ADS1115"},
        {0x68, "MPU6050"},
        {0x76, "BMP280"},
        {0x77, "BMP280"}};

/* =====================================================
   NAME LOOKUP
===================================================== */

const char *getI2CName(uint8_t addr)
{
    for (const auto &d : knownDevices)
    {
        if (d.addr == addr)
            return d.name;
    }
    return nullptr;
}

/* =====================================================
   SCAN
===================================================== */

void scan_i2c_devices()
{
    Serial.println("\n========== I2C SCAN ==========");

    // reset state (IMPORTANT)
    i2cCount = 0;
    lastI2CAddr = 0;

    for (int i = 0; i < 128; i++)
        i2cDevices[i] = false;

    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();

        if (err == 0)
        {
            i2cDevices[addr] = true;
            i2cCount++;
            lastI2CAddr = addr;

            const char *name = getI2CName(addr);

            if (name)
                Serial.printf("0x%02X  OK   %s\n", addr, name);
            else
                Serial.printf("0x%02X  OK   UNKNOWN\n", addr);
        }
    }

    const char *lastName = getI2CName(lastI2CAddr);

    Serial.printf("[I2C] Total devices: %u\n", i2cCount);

    if (lastI2CAddr)
        Serial.printf("[I2C] Last: 0x%02X %s\n",
                      lastI2CAddr,
                      lastName ? lastName : "UNKNOWN");

    Serial.println("================================\n");
}