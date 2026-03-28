#!/bin/sh
# /mnt/data/mqtt_web.sh
# Show latest MQTT readings (CGI), sorted by sensor/metric

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
awk -F',' '
{
    key = $1
    if (!(key in ts) || $2 > ts[key]) {
        ts[key] = $2
        val[key] = $3
    }
}
END {
    for (k in ts)
        print k "," ts[k] "," val[k]
}
' "$LOGDIR"/*_"$TODAY".log 2>/dev/null |
# split fields, add Sensor/Metric as sort keys
awk -F',' '{ 
    split($1, a, "/"); 
    print a[1] "," a[2] "," a[3] "," $3 "," $2 
}' |
# sort by Sensor, then Metric
sort -t',' -k2,2 -k3,3 |
while IFS=',' read -r LOCATION SENSOR METRIC VALUE TS
do
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
