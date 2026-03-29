## ESP32 Firmware Setup

### 1. Add and edit WiFi secrets

```bash
cp firmware/esp32_logger/wifi_secrets_example.h \
   firmware/esp32_logger/wifi_secrets.h

nano firmware/esp32_logger/wifi_secrets.h
```
---

## Install Arduino CLI
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo install -m 0755 bin/arduino-cli /usr/local/bin/arduino-cli
```

Initialize configuration:
```bash
arduino-cli config init
arduino-cli core update-index
```

## Install ESP32 board and libraries

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install \
arduino-cli lib install "AsyncTCP" || true
arduino-cli lib install "ESP Async WebServer" || true
arduino-cli lib install "PubSubClient" || true
arduino-cli lib install "Adafruit SHT4x Library" || true
arduino-cli lib install "Adafruit BMP280 Library" || true
arduino-cli lib install "Adafruit TSL2591 Library" || true
arduino-cli lib install "SparkFun SCD4x Arduino Library" || true
```
---
## Compile firmware
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --verbose firmware/esp32_logger
```
---
## Upload firmware
```bash
PORT=/dev/ttyUSB0
arduino-cli upload -p $PORT --fqbn esp32:esp32:esp32 firmware/esp32_logger
```
---
## Monitor serial
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200


## Optional: Makefile 
```bash
make compile
make upload # usb
make upload-ota # wifi
```



### HARDWARE

## BOARD
ESP32 ($5)
5VIN 3VOUT
https://documentation.espressif.com/esp32-wroom-32_datasheet_en.pdf


https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html#id16


PINOUT
https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/_images/esp32_devkitC_v4_pinlayout.png



ESP32-WROOM-32
A module with ESP32 at its core. For more information, see ESP32-WROOM-32 Datasheet.

EN
Reset button.

Boot
Download button. Holding down Boot and then pressing EN initiates Firmware Download mode for downloading firmware through the serial port.

USB-to-UART Bridge
Single USB-UART bridge chip provides transfer rates of up to 3 Mbps.

Micro USB Port
USB interface. Power supply for the board as well as the communication interface between a computer and the ESP32-WROOM-32 module.

5V Power On LED
Turns on when the USB or an external 5V power supply is connected to the board. For details see the schematics in Related Documents.

I/O
Most of the pins on the ESP module are broken out to the pin headers on the board. You can program ESP32 to enable multiple functions such as PWM, ADC, DAC, I2C, I2S, SPI, etc.

There are three mutually exclusive ways to provide power to the board:
    Micro USB port, default power supply
    5V and GND header pins
    3V3 and GND header pins

Use one and only one of the options above, otherwise the board and/or the power supply source can be damaged.


## SENSORS
LD1020 Human Microwave Radar ($2)   
5VIN 3VOUT
https://rajguruelectronics.com/Product/21153/170624162821.pdf

SHT40 Temperature, Humidity ($2)
3V
https://sensirion.com/products/catalog/SHT40

SCD40 CO2 ($20)
3V
https://sensirion.com/products/catalog/SCD40

BMP280 Pressure ($2)
3V
https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf

TSL2591 Light ($4)
3V
https://cdn-shop.adafruit.com/datasheets/TSL25911_Datasheet_EN_v1.pdf

