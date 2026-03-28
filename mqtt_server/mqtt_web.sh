#!/bin/sh
# /mnt/data/mqtt_web.sh
# Show latest MQTT readings (CGI)

LOGDIR="/mnt/data/logs"
TODAY=$(date +%Y-%m-%d)

# ---------------- HTTP header ----------------
printf "Content-type: text/html\n\n"

cat <<EOF
<html>
<head>
<title>Sensor Readings</title>
<meta http-equiv="refresh" content="5">
<style>
body{font-family:sans-serif}
table{border-collapse:collapse}
td,th{border:1px solid #444;padding:4px}
th{background:#eee}
</style>
</head>
<body>
<h2>Latest Sensor Readings</h2>
<table>
<tr>
<th>Room</th>
<th>Sensor</th>
<th>Metric</th>
<th>Value</th>
<th>Time</th>
</tr>
EOF

# ---------------- collect newest values ----------------
awk -F',' -v today="$TODAY" '

# keep only newest record per location/sensor/metric
{
    key = $2 "|" $3 "|" $4
    data[key] = $0
}

END {
    for (k in data)
        print data[k]
}

' "$LOGDIR"/*_"$TODAY".log 2>/dev/null |
while IFS=',' read -r TS LOCATION SENSOR METRIC VALUE
do
    # portable timestamp conversion (busybox + GNU)
    TIME=$(date -d "@$TS" "+%F %T" 2>/dev/null || date -r "$TS" "+%F %T")

    printf "<tr>"
    printf "<td>%s</td>" "$LOCATION"
    printf "<td>%s</td>" "$SENSOR"
    printf "<td>%s</td>" "$METRIC"
    printf "<td>%s</td>" "$VALUE"
    printf "<td>%s</td>" "$TIME"
    printf "</tr>\n"
done

cat <<EOF
</table>
</body>
</html>
EOF