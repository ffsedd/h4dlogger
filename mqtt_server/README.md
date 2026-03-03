# MQTT Topic Syntax

## Format
    <location>/<device>/<sensor>/<metric>


## Examples

Temperature
    kitchen/lab/sht40/temp

Humidity
    kitchen/lab/sht40/rh

Pressure
    kitchen/lab/bmp280/pressure

Light
    kitchen/lab/tsl2591/lux

## Wildcards

All sensors in room
    kitchen/lab/+/+

All temperature sensors
    +/+/+/temp

Everything in kitchen
    kitchen/#

## Payload

Single numeric value:
    23.41

Units are implicit:

  metric     unit
  ---------- ------
  temp       °C
  rh         \%
  pressure   Pa
  lux        lx

## Naming rules

-   lowercase only
-   short identifiers
-   no spaces
-   stable topics
