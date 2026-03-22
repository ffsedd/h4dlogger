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

---

## Install ESP32 board support

```bash
arduino-cli core install esp32:esp32
```

---

## Install required Arduino libraries

```bash
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

For **ESP32-C3 boards**:

```bash
arduino-cli compile \
--fqbn esp32:esp32:esp32 \
--verbose \
firmware/esp32_logger_v11
```

---

## Upload firmware

```bash
PORT=/dev/ttyUSB0

arduino-cli upload \
-p $PORT \
--fqbn esp32:esp32:esp32 \
firmware/esp32_logger_v11
```

---
## Monitor serial

arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200



## Optional: Makefile (simpler workflow)

Create `firmware/Makefile`:

```make
BOARD=esp32:esp32:esp32
PORT=/dev/ttyUSB0
SKETCH=firmware/esp32_logger

compile:
	arduino-cli compile --fqbn $(BOARD) $(SKETCH)

upload:
	arduino-cli upload -p $(PORT) --fqbn $(BOARD) $(SKETCH)
```

Then run:

```bash
make compile
make upload
```
