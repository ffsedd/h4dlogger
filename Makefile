BOARD=esp32:esp32:esp32
PORT=/dev/ttyUSB0
SKETCH=firmware/esp32_logger

compile:
	arduino-cli compile --verbose --fqbn $(BOARD) $(SKETCH)

upload:
upload:
	arduino-cli upload \
	-p /dev/ttyUSB0 \
	--fqbn esp32:esp32:esp32 \
	--upload-property upload.speed=460800 \
	firmware/esp32_logger

	arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
