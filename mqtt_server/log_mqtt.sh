#!/bin/sh
#
# mqtt_logger.sh
#
# MQTT → append-only timeseries logger
#
# INPUT:
#   topic   device/sensor/metric
#   payload timestamp,value
#
# OUTPUT RECORD:
#   ts,device,sensor,metric,value
#

BROKER="localhost"
TOPIC="#"
LOGDIR="/mnt/data/logs"

mkdir -p "$LOGDIR"

exec mosquitto_sub \
    -h "$BROKER" \
    -t "$TOPIC" \
    -v \
    -q 1 \
| awk -v LOGDIR="$LOGDIR" '

BEGIN {
    FS=" "
    OFS=","
}

{
    # ------------------------------------------------
    # Split mosquitto_sub line safely
    # topic payload(with commas allowed)
    # ------------------------------------------------
    topic = $1

    $1=""
    sub(/^ /,"")
    payload=$0

    # ------------------------------------------------
    # Parse topic → device/sensor/metric
    # ------------------------------------------------
    n = split(topic, t, "/")

    device = (n>=1?t[1]:"device")
    sensor = (n>=2?t[2]:"sensor")
    metric = (n>=3?t[3]:"metric")

    if (n>3) {
        for(i=4;i<=n;i++)
            metric = metric "_" t[i]
    }

    # ------------------------------------------------
    # Parse CSV payload: timestamp,value
    # ------------------------------------------------
    ts=""
    value=""

    m = split(payload, p, ",")

    if (m>=2) {
        ts    = p[1]
        value = p[2]
    }

    # fallback if timestamp missing
    if (ts=="")
        ts=systime()

    # ------------------------------------------------
    # Daily partition
    # ------------------------------------------------
    day  = strftime("%Y-%m-%d", ts)
    file = LOGDIR "/" device "_" day ".log"

    # ------------------------------------------------
    # Write normalized CSV
    # ------------------------------------------------
    print ts,device,sensor,metric,value >> file

    fflush(file)
}
'