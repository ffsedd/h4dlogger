BOARD=esp32:esp32:nologo_esp32c3_super_mini
PORT=/dev/ttyACM0
SKETCH=esp32c3supermini_logger.ino
BAUDRATE=115200
UPLOADSPEED=460800
OTA_IP=10.11.12.181   # ESP32 IP on your Wi-Fi network

compile:
	arduino-cli compile --verbose --fqbn $(BOARD) $(SKETCH)

upload:
	echo "Uploading to $(PORT) at $(BAUDRATE) baud..."
	arduino-cli upload -p $(PORT)  --fqbn $(BOARD) --upload-property upload.speed=$(UPLOADSPEED) $(SKETCH)
	arduino-cli monitor -p $(PORT)-c baudrate=$(BAUDRATE)

upload-ota:
	echo "Uploading to $(OTA_IP)  ..."
	echo "\n\n------------------------ PRESS ENTER ------------------------\n\n"
	arduino-cli upload --fqbn $(BOARD) -p $(OTA_IP) $(SKETCH)
