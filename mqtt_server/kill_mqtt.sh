#!/bin/sh
pids=$(ps w | grep '[l]og_mqtt.sh' | awk '{print $1}')

[ -z "$pids" ] && echo "No processes found" && exit 0

for pid in $pids; do
    echo "Killing PID $pid"
    kill "$pid"
done
