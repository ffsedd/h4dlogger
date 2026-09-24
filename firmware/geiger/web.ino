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

static constexpr int HISTORY = 3600; // ~60 min @ 1Hz
static float cpsHistory[HISTORY];

static int head = 0;
static bool filled = false;

/* =========================
   HISTORY BUFFER
========================= */

static inline void pushSample(float cps)
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
    const int size = filled ? HISTORY : head;
    const int start = filled ? head : 0;

    out += "\"cpsData\":[";

    for (int i = 0; i < size; i++)
    {
        const int idx = (start + i) % HISTORY;
        out += String(cpsHistory[idx], 0);

        if (i + 1 < size)
            out += ",";
    }

    out += "]";
}

/* =========================
   JSON API
========================= */

static String buildJSON()
{
    pushSample(status.geiger_cps);

    String out;
    out.reserve(2048);

    out += "{";

    /* ===== GEIGER ===== */
    out += "\"cps\":" + String(status.geiger_cps, 0) + ",";
    out += "\"cps_smooth\":" + String(status.geiger_cps_smooth, 3) + ",";
    out += "\"total\":" + String((uint64_t)status.geiger_total_count) + ",";

    /* ===== WIFI ===== */
    out += "\"wifi_rssi\":" + String(status.wifi_rssi) + ",";
    out += "\"wifi_tx_power\":" + String(status.wifi_tx_power, 2) + ",";
    out += "\"wifi_connected\":" + String(status.wifi_connected ? "true" : "false") + ",";

    /* ===== SYSTEM ===== */
    out += "\"cpu_temp\":" + String(status.cpu_temp, 1) + ",";

    /* ===== SAFETY ===== */
    out += "\"alarm\":" + String(status.alarm_active ? "true" : "false") + ",";

    /* ===== HISTORY ===== */
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
<title>Geiger Monitor</title>
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
    font-size:14px;
    color:#9fb3c8;
    margin-bottom:10px;
}

canvas{
    background:#0f1722;
    border-radius:12px;
    padding:12px;
}
</style>
</head>

<body>

<h2>Geiger Counter Dashboard</h2>

<div class="header">
    <div>CPS <span id="cps" class="value">0</span></div>
    <div>AVG <span id="mean" class="value" style="color:#ffcc00">0</span></div>
</div>

<div class="status">
RSSI: <span id="rssi">0</span> dBm |
TEMP: <span id="temp">0</span> °C |
TOTAL: <span id="total">0</span> |
WIFI: <span id="wifi">-</span> |
ALARM: <span id="alarm">-</span>
</div>

<canvas id="chart"></canvas>
<script>
const ctx = document.getElementById('chart');

const chart = new Chart(ctx, {
    type: 'bar',
    data: {
        labels: [],
        datasets: [{
            label: 'CPS',
            data: [],
            backgroundColor: 'rgba(0, 229, 255, 0.35)',
            borderWidth: 0
        }]
    },
    options: {
        animation: false,
        responsive: true,
        scales: {
            x: {
                display: false
            },
            y: {
                beginAtZero: true,
                ticks: {
                    color: '#9fb3c8'
                }
            }
        },
        plugins: {
            legend: {
                display: false
            }
        }
    }
});

async function update()
{
    try {
        const d = await (await fetch('/data')).json();

        document.getElementById('cps').innerText = d.cps.toFixed(0);
        document.getElementById('mean').innerText = d.cps_smooth.toFixed(3);

        document.getElementById('rssi').innerText = d.wifi_rssi;
        document.getElementById('temp').innerText = d.cpu_temp.toFixed(1);
        document.getElementById('total').innerText = d.total;

        document.getElementById('wifi').innerText = d.wifi_connected ? "OK" : "NO";
        document.getElementById('alarm').innerText = d.alarm ? "YES" : "NO";

        if (d.cpsData)
        {
            chart.data.labels = d.cpsData.map((_, i) => i);
            chart.data.datasets[0].data = d.cpsData;

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
    if (WiFi.status() == WL_CONNECTED)
    {
        server.handleClient();
    }
}