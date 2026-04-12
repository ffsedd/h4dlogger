############################################################
# USER CONFIG
############################################################

BOARD        := esp32:esp32:nologo_esp32c3_super_mini
SKETCH       := esp32c3supermini_logger.ino
PORT         := /dev/ttyACM0
BAUDRATE     := 115200
UPLOADSPEED  := 460800

ID           ?= playroom
IP           ?= 10.11.12.181

CONFIG_FILE  := build_config.h

############################################################
# TARGETS
############################################################

.PHONY: compile upload monitor ota ls clean

############################################################
# CONFIG FILE (MAKE-BASED DEPENDENCY)
############################################################

config:
	@echo "Generating $(CONFIG_FILE) for ID=$(ID)"
	@printf '#pragma once\n#define DEVICE_ID "%s"\n' "$(ID)" > $(CONFIG_FILE)

############################################################
# COMPILE (depends on config file)
############################################################

compile: $(CONFIG_FILE)
	@echo "Compiling ID=$(ID)"
	arduino-cli compile --verbose \
		--fqbn $(BOARD) \
		$(SKETCH)

############################################################
# UPLOAD
############################################################

upload: 
	@echo "Uploading via SERIAL → $(PORT)"
	arduino-cli upload --verbose \
		-p $(PORT) \
		--fqbn $(BOARD) \
		--upload-property upload.speed=$(UPLOADSPEED) \
		$(SKETCH)

monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUDRATE)

ota: 
	@echo "Uploading OTA → $(IP)"
	arduino-cli upload --verbose \
		-p $(IP) \
		--fqbn $(BOARD) \
		$(SKETCH)

ls:
	arduino-cli board list

clean:
	rm -rf build $(CONFIG_FILE)
