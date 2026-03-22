#!/bin/sh
# /mnt/data/mqtt_web.sh
# CGI script to show latest readings

LOGDIR="/mnt/data/logs"

# Print HTTP header
echo "Content-type: text/html"
echo ""

# HTML header
echo "<html><head><title>Sensor Readings</title>"
echo "<meta http-equiv='refresh' content='5'>"
echo "<style>table{border-collapse:collapse}td,th{border:1px solid black;padding:4px}</style>"
echo "</head><body>"
echo "<h2>Latest Sensor Readings</h2>"
echo "<table><tr><th>Room</th><th>Sensor</th><th>Metric</th><th>Value</th><th>Time</th></tr>"

# Loop through room logs
for LOG in "$LOGDIR"/*_$(date +%Y-%m-%d).log; do
    [ -f "$LOG" ] || continue
    ROOM=$(basename "$LOG" | cut -d_ -f1)
    # Read latest line per topic
    awk '{data[$2]=$0} END{for(t in data) print data[t]}' "$LOG" | while read -r TS TOPIC VALUE; do
        SENSOR=$(echo "$TOPIC" | cut -d/ -f2)
        METRIC=$(echo "$TOPIC" | cut -d/ -f3)
        TIME=$(date -d "@$TS" "+%Y-%m-%d %H:%M:%S" 2>/dev/null || date -r $TS "+%Y-%m-%d %H:%M:%S")
        echo "<tr><td>$ROOM</td><td>$SENSOR</td><td>$METRIC</td><td>$VALUE</td><td>$TIME</td></tr>"
    done
done

echo "</table></body></html>"
