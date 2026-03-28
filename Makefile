BOARD=esp32:esp32:esp32
PORT=/dev/ttyUSB0
SKETCH=firmware/esp32_logger_v11
BAUDRATE=115200
UPLOADSPEED=460800
OTA_IP=10.11.12.125   # ESP32 IP on your Wi-Fi network

compile:
	arduino-cli compile --verbose --fqbn $(BOARD) $(SKETCH)

upload:
	echo "Uploading to $(PORT) at $(BAUDRATE) baud..."
	arduino-cli upload -p $(PORT)  --fqbn $(BOARD) --upload-property upload.speed=$(UPLOADSPEED) $(SKETCH)
	arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=$(BAUDRATE)

upload-ota:
	echo "Uploading to $(OTA_IP)  ..."
	echo "Password empty, just press Enter if prompted."
	arduino-cli upload --fqbn $(BOARD) -p $(OTA_IP) $(SKETCH)
