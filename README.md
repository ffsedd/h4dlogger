# h4dlogger Repository

**Environmental data logging system** with ESP32 sensors, MQTT telemetry, and Python analysis tools for flexible monitoring and visualization.

---

## Architecture

```
ESP32 Sensors (SHT40, BMP280, TSL2591)
       │
       │ MQTT (kitchen/lab/…)
       ▼
   MQTT Broker
       │
       ▼
   mqtt-capture.sh (raw logs)
       │
       ▼
Python Analysis Tools (h4dlogger)
       │
       ▼
   Plots & Metrics
```

---

## Repository Structure

```
.
├── bin/                 # Local tools (arduino-cli)
├── firmware/            # ESP32 firmware and configuration
├── mqtt_server/         # MQTT logging scripts for server
├── scripts/             # Helper shell scripts
├── src/h4dlogger/       # Python data analysis package
├── Makefile             # Build & run targets
├── pyproject.toml       # Python project config
└── uv.lock              # Dependency lockfile
```

---

## MQTT Topic Syntax

Hierarchical, human-readable topics:
```
location/device/sensor/metric
```

Example:
```
kitchen/lab/sht40/temp
kitchen/lab/sht40/rh
kitchen/lab/bmp280/pressure
kitchen/lab/tsl2591/lux
```

Full specification: [MQTT Topic Syntax](mqtt_topic_syntax.md)

### Wildcards
- All sensors in a room: `kitchen/lab/+/+`
- All temperature sensors: `+/+/+/temp`
- All kitchen data: `kitchen/#`

---

## Components

### ESP32 Firmware (`firmware/esp32_logger`)
- Auto-detects supported sensors
- Publishes data via MQTT
- Async web dashboard (fast and responsive)
- OTA firmware updates
- WiFi watchdog recovery
- Configuration: `wifi_secrets.h`
- Documentation: `firmware/README.md`

### MQTT Server Logging (`mqtt_server/`)
- `mqtt-capture.sh`: capture MQTT messages into logs
- `etc_init.d_mqttlog.sh`: service startup script
- Documentation: `mqtt_server/README.md`

### Python Analysis (`src/h4dlogger/`)
Modules:
| Module | Purpose |
|--------|---------|
| loader.py | Load raw MQTT logs |
| parser.py | Parse topics and values |
| metrics.py | Compute derived metrics (dew point, etc.) |
| plot.py | Plot graphs and charts |
| main.py | CLI entry point |

Usage:
```
uv run dlogger plot logs/*.log
```
or
```
scripts/dlogger.sh
```

### Makefile
Common commands:
```
make firmware-build
make firmware-upload
make mqtt-start
make mqtt-stop
```

### Installation
```
uv sync      # Install Python dependencies
uv run dlogger  # Run CLI
```

---

## Design Goals
- Minimal overhead MQTT schema
- Append-only raw logs for reproducibility
- Efficient Python analysis and plotting
- Scalable to multiple devices and sensors
- Robust ESP32 firmware with recovery mechanisms

---

## License
MIT
