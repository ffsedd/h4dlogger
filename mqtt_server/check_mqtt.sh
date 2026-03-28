#!/bin/sh
echo check service status
echo ---------------------------
echo /etc/init.d/mqtt_log_service status
/etc/init.d/mqtt_log_service status
echo "ps | grep mqtt"
ps | grep mqtt
echo check mqtt messages
echo ---------------------------
mosquitto_sub -h localhost -t '#' -C 1 -v
