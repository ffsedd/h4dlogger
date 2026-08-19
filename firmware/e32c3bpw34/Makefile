############################################################
# USER CONFIG
############################################################

BOARD        := esp32:esp32:nologo_esp32c3_super_mini
SKETCH       := e32c3bpw34.ino
PORT         := /dev/ttyACM0
BAUDRATE     := 115200
UPLOADSPEED  := 460800

ID           ?= bpw34a
IP           ?= 10.11.12.201

CONFIG_FILE  := build_config.h

############################################################
# INTELLISENSE
############################################################

BUILD_DIR := build
CC_JSON   := compile_commands.json

############################################################
# TARGETS
############################################################

.PHONY: config compile upload monitor ota flash ls clean ccjson ide all

############################################################
# CONFIG FILE
############################################################

config:
	@echo "Generating $(CONFIG_FILE) for ID=$(ID)"
	@printf '#pragma once\n#define DEVICE_ID "%s"\n' "$(ID)" > $(CONFIG_FILE)

############################################################
# COMPILE
############################################################

compile: $(CONFIG_FILE)
	@echo "Compiling..."
	cat $(CONFIG_FILE)

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

############################################################
# MONITOR
############################################################

monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUDRATE)

############################################################
# OTA UPLOAD
############################################################

ota:
	@echo "Uploading OTA → $(IP)"
	arduino-cli upload --verbose \
		-p $(IP) \
		--fqbn $(BOARD) \
		$(SKETCH)

############################################################
# COMBO FLASH
############################################################

flash: compile upload
	@sleep 1
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUDRATE)

############################################################
# BOARD LIST
############################################################

ls:
	arduino-cli board list

############################################################
# CLEAN
############################################################

clean:
	rm -rf ~/.cache/arduino/sketches
	rm -rf $(BUILD_DIR)
	rm -f $(CONFIG_FILE)
	rm -f $(CC_JSON)

############################################################
# INTELLISENSE GENERATION
############################################################

ccjson: $(CONFIG_FILE)
	@echo "Generating compile_commands.json..."

	@rm -rf $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)

	arduino-cli compile --verbose \
		--fqbn $(BOARD) \
		--build-path $(BUILD_DIR) \
		--export-binaries \
		$(SKETCH)

	@echo "Searching compile_commands.json..."
	@CC=$$(find $(BUILD_DIR) -name compile_commands.json | head -n 1); \
	if [ -z "$$CC" ]; then \
		echo "ERROR: compile_commands.json not found"; \
		exit 1; \
	fi; \
	cp $$CC $(CC_JSON)

	@echo "OK → $(CC_JSON) ready"

############################################################
# VS CODE READY TARGET
############################################################

ide: ccjson
	@echo "VS Code IntelliSense ready"

############################################################
# FULL WORKFLOW
############################################################

all: ccjson flash
