#!/bin/sh

case "$1" in
start)
    /mnt/data/mqtt_logger.sh &
    ;;
stop)
    killall mosquitto_sub
    ;;
esac

