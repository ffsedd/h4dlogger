BOARD=esp32:esp32:esp32
PORT=/dev/ttyUSB0
SKETCH=firmware/esp32_logger

compile:
	arduino-cli compile --verbose --fqbn $(BOARD) $(SKETCH)

upload:
	arduino-cli upload -p $(PORT) --fqbn $(BOARD) $(SKETCH)
	arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
