#include <WebServer.h>
#include "status.h"

WebServer server(80);

/* =========================
   EXTERNAL STATE
========================= */

extern Status status;

/* =========================
   LOCAL WEB CACHE (UI OWNED)
========================= */

static constexpr int HISTORY = 600; // ~10 min @ 1Hz
static float cpsHistory[HISTORY];

static int head = 0;
static bool filled = false;

/* =========================
   HISTORY BUFFER
========================= */

static void pushSample(float cps)
{
    cpsHistory[head++] = cps;

    if (head >= HISTORY)
    {
        head = 0;
        filled = true;
    }
}

static void buildHistoryArray(String &out)
{
    int size = filled ? HISTORY : head;
    int start = filled ? head : 0;

    out += "\"cpsData\":[";

    for (int i = 0; i < size; i++)
    {
        int idx = (start + i) % HISTORY;
        out += String(cpsHistory[idx], 0);

        if (i + 1 < size)
            out += ",";
    }

    out += "]";
}

/* =========================
   JSON API
========================= */

String buildJSON()
{
    // update web cache
    pushSample(status.geiger_cps);

    String out;
    out.reserve(800);

    out += "{";

    out += "\"cps\":" + String(status.geiger_cps, 0) + ",";
    out += "\"mean\":" + String(status.geiger_cps_smooth, 3) + ",";
    out += "\"rssi\":" + String(status.wifi_rssi) + ",";
    out += "\"cpuTemp\":" + String(status.cpu_temp, 1) + ",";
    out += "\"total\":" + String((uint64_t)status.geiger_total_count) + ",";

    out += "\"wifi\":" + String(status.wifi_connected ? "true" : "false") + ",";
    out += "\"alarm\":" + String(status.alarm_active ? "true" : "false") + ",";

    buildHistoryArray(out);

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
    font-size:13px;
    color:#9fb3c8;
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

<div class="header">
    <div>CPS <span id="cps" class="value">0</span></div>
    <div>AVG <span id="mean" class="value" style="color:#ffcc00">0</span></div>
</div>

<div class="status">
RSSI: <span id="rssi">0</span> |
TEMP: <span id="temp">0</span> °C
</div>

<canvas id="chart"></canvas>

<script>
const ctx = document.getElementById('chart');

const chart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'CPS',
            data: [],
            borderColor: '#00e5ff',
            pointRadius: 0,
            tension: 0.2
        }]
    },
    options: {
        animation: false,
        responsive: true,
        scales: {
            x: { display: false },
            y: { beginAtZero: true }
        }
    }
});

async function update()
{
    try {
        const d = await (await fetch('/data')).json();

        document.getElementById('cps').innerText = d.cps.toFixed(0);
        document.getElementById('mean').innerText = d.mean.toFixed(3);
        document.getElementById('rssi').innerText = d.rssi + " dBm";
        document.getElementById('temp').innerText = d.cpuTemp.toFixed(1);

        if (d.cpsData)
        {
            chart.data.datasets[0].data = d.cpsData;
            chart.data.labels = d.cpsData.map((_, i) => i);

            chart.update('none');
        }
    }
    catch (e)
    {
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