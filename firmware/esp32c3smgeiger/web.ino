#include <WebServer.h>
#include "utils.h"
WebServer server(80);

/* =========================
   EXTERNAL STATE
========================= */

extern float cps;
extern float cpsMean;

extern float cpsBuffer[];
extern float avgBuffer[];

extern uint32_t bufIndex;
extern bool bufFull;

extern uint32_t getBufferSize();

/* =========================
   JSON API
========================= */
String buildJSON()
{
    uint32_t n = getBufferSize();
    uint32_t start = bufFull ? bufIndex : 0;

    String out;
    out.reserve(2500);

    out += "{";

    /* =========================
       LIVE VALUES (UI)
    ========================= */
    out += "\"cps\":" + String(cps, 2) + ",";
    out += "\"mean\":" + String(cpsMean, 3) + ",";
    out += "\"rssi\":" + String(getRSSI()) + ",";
    out += "\"cpuTemp\":" + String(getCpuTemp(), 1) + ",";

    /* =========================
       CPS SERIES
    ========================= */
    out += "\"cpsData\":[";

    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t idx = (start + i) % CPS_WINDOW;
        out += String(cpsBuffer[idx], 3);

        if (i + 1 < n)
            out += ",";
    }

    out += "],";

    /* =========================
       MEAN SERIES
    ========================= */
    out += "\"meanData\":[";

    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t idx = (start + i) % CPS_WINDOW;
        out += String(avgBuffer[idx], 3);

        if (i + 1 < n)
            out += ",";
    }

    out += "]";

    out += "}";

    return out;
}
/* =========================
   UI
========================= */

void handleRoot()
{
    server.send(200, "text/html", R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>Geiger CPS</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>
body{
    background:#0b0f14;
    color:#cfd8dc;
    font-family:monospace;
}

.header{
    display:flex;
    gap:40px;
    font-size:34px;
    margin-bottom:10px;
}

.value{
    font-weight:bold;
    font-size:48px;
    color:#00e5ff;
}
.status{
    margin-top:8px;
    font-size:13px;
    color:#9fb3c8;
    border-collapse:collapse;
}

.status td{
    padding:2px 10px 2px 0;
}

.status td:first-child{
    opacity:0.7;
}

.status td:last-child{
    color:#00ff99;
    text-align:right;
    font-weight:bold;
}
canvas{
    background:#0f1722;
    border-radius:12px;
    padding:12px;
}
</style>
</head>

<body>

<h2>Geiger CPS Monitor</h2>
<table class="status">
<tr><td>RSSI</td><td id="rssi">0</td></tr>
<tr><td>CPU TEMP</td><td id="temp">0</td></tr>
</table>

<div class="header">
    <div>CPS <span id="cps" class="value">0</span></div>
    <div>MEAN <span id="mean" class="value" style="color:#ffcc00">0</span></div>
</div>



<canvas id="chart"></canvas>

<script>
const ctx = document.getElementById('chart');

const chart = new Chart(ctx, {
    data: {
        labels: [],
        datasets: [

        {
            type: 'bar',
            label: 'CPS',
            data: [],
            backgroundColor: 'rgba(0,229,255,0.25)',
            borderWidth: 0
        },

        {
            type: 'line',
            label: 'mean',
            data: [],
            borderColor: '#ffcc00',
            pointRadius: 0,
            tension: 0.35,
            borderWidth: 2
        }

        ]
    },
    options: {
        animation: false,
        responsive: true,
        scales: {
            x: { display: false },
            y: {
                beginAtZero: true,
                ticks: {
                    font: {
                        size: 16
                    }
                }
            }
        }
    }
});

async function update()
{
    try {
        const d = await (await fetch('/data')).json();

        document.getElementById('cps').innerText = d.cps.toFixed(2);
        document.getElementById('mean').innerText = d.mean.toFixed(3);
        document.getElementById('rssi').innerText = d.rssi + " dBm";
        document.getElementById('temp').innerText = d.cpuTemp.toFixed(1) + " °C";

        const n = d.cpsData.length;

        chart.data.labels = Array.from({length:n}, (_,i)=>i);
        chart.data.datasets[0].data = d.cpsData;
        chart.data.datasets[1].data = d.meanData;

        chart.update('none');
    }
    catch (e) {
        console.log("update failed", e);
    }
}

setInterval(update, 1000);
update();
</script>

</body>
</html>
)rawliteral");
}

/* =========================
   API
========================= */

void handleData()
{
    server.send(200, "application/json", buildJSON());
}

/* =========================
   PUBLIC
========================= */

void startWeb()
{
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();

    Serial.println("Web server started");
}

void handleWebLoop()
{
    server.handleClient();
}
