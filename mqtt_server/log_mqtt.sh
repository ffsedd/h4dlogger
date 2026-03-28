#!/bin/sh
#
# mqtt_logger_openwrt.sh - MQTT → daily room logs for BusyBox/OpenWrt
#

BROKER="localhost"
TOPIC="#"
LOGDIR="/mnt/data/logs"

mkdir -p "$LOGDIR"

mosquitto_sub -h "$BROKER" -t "$TOPIC" -v | awk -v LOGDIR="$LOGDIR" '
BEGIN { FS=" "; OFS="," }
{
    # $1 = topic, $2 = payload
    split($2,a,",")        # a[1]=timestamp, a[2]=value
    split($1,t,"/")        # t[1]=room
    room=t[1]

    # Get daily filename (date from timestamp)
    cmd = "date -d @" a[1] " +%F 2>/dev/null"
    cmd | getline day
    close(cmd)
    if(day=="") { day="unknown" }   # fallback if date fails

    file = LOGDIR "/" room "_" day ".log"
    print $1, a[1], a[2] >> file
    close(file)
}'
