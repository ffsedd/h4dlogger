#!/bin/sh

LOGDIR="/mnt/data/logs"
mkdir -p "$LOGDIR"

mosquitto_sub -h localhost -t "#" -v | \
while read -r TOPIC PAYLOAD
do
    TS=$(date +%s)
    DATE=$(date +%Y-%m-%d)

    TYPE=${TOPIC%%/*}
    REST=${TOPIC#*/}
    ID=${REST%%/*}

    printf "%s %s %s\n" "$TS" "$TOPIC" "$PAYLOAD" \
        >> "$LOGDIR/${TYPE}_${ID}_${DATE}.log"

done
