BOARD=esp32:esp32:esp32c3
PORT=/dev/ttyACM0
SKETCH=esp32c3_sht21.ino

compile:
	arduino-cli compile --verbose --fqbn $(BOARD) $(SKETCH)

upload:
	arduino-cli upload \
		--verbose \
		-p $(PORT) \
		--fqbn $(BOARD) \
		--upload-property upload.speed=115200 \
		$(SKETCH)
	arduino-cli monitor -p $(PORT) -c baudrate=115200
monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=115200
