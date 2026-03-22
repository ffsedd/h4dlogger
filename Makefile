BOARD=esp32:esp32:esp32
PORT=/dev/ttyUSB0
SKETCH=firmware/esp32_logger_v11
BAUDRATE=115200
UPLOADSPEED=460800

compile:
	arduino-cli compile --verbose --fqbn $(BOARD) $(SKETCH)

upload:
	arduino-cli upload -p $(PORT)  --fqbn $(BOARD) --upload-property upload.speed=$(UPLOADSPEED) $(SKETCH)
	arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=$(BAUDRATE)
