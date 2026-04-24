# PINOUT


# ESP32-C3 SuperMini — Pin Overview

## LEFT (usb up)
| GPIO5 | GPIO, ADC1_CH5, PWM | Default SPI MISO / SPI SS |
| GPIO6 | GPIO, PWM | Default SPI MOSI |
| GPIO7 | GPIO, PWM | Default SPI SS |
| GPIO8 | GPIO, PWM | Onboard LED (active LOW), **Strapping pin**, I2C SDA |
| GPIO9 | GPIO, PWM | BOOT button, **Strapping pin**, I2C SCL |
| GPIO10 | GPIO, PWM | Default SPI SCK |
| GPIO20 | GPIO, PWM | Default UART RX |
| GPIO21 | GPIO, PWM | Default UART TX |

## RIGHT (usb up)
| 5V | 5V input/output | Connected to USB-C 5V rail |
| GND | Ground | Common ground |
| 3V3 | 3.3V input/output | From onboard regulator or external regulated 3.3V supply |
| GPIO4 | GPIO, ADC1_CH4, PWM | Default SPI SCK |
| GPIO3 | GPIO, ADC1_CH3, PWM | Analog capable |
| GPIO2 | GPIO, ADC1_CH2, PWM | **Strapping pin (boot mode)** — avoid |
| GPIO1 | GPIO, ADC1_CH1, PWM | Analog capable |
| GPIO0 | GPIO, ADC1_CH0, PWM | Analog capable |

---

## Strapping Pins

| Pin | Role | Recommendation |
|-----|------|---------------|
| GPIO2 | Boot mode selection | Avoid general use |
| GPIO8 | Onboard LED (active LOW) + boot strap | Use carefully |
| GPIO9 | BOOT button + boot strap | Avoid |

Strapping pins change state during reset and bootloader entry.

---

## PWM
All general-purpose GPIO pins support PWM output.

---

## Default Peripheral Mapping (Arduino Core)

### UART
| Signal | GPIO |
|--------|-----|
| RX | GPIO20 |
| TX | GPIO21 |

### SPI
| Signal | GPIO |
|--------|-----|
| MISO | GPIO6 |
| MOSI | GPIO7 |
| SCK | GPIO10 |
| SS | GPIO5 |

### I2C
| Signal | GPIO |
|--------|-----|
| SDA | GPIO8 |
| SCL | GPIO9 |

---

## Notes

- ESP32-C3 supports flexible pin multiplexing; interfaces can be remapped.
- Avoid strapping pins for critical peripherals when possible.
- Onboard LED is inverted logic (**LOW = ON**).
