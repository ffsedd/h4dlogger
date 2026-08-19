#include "web.h"

#include <WebServer.h>
#include <WiFi.h>

#include "status.h"

WebServer server(80);

/* =========================
   LOCAL WEB CACHE (UI OWNED)
========================= */

static constexpr int HISTORY = 3600; // ~60 min @ 1Hz
static float signalHistory[HISTORY];

static int head = 0;
static bool filled = false;

/* =========================
   HISTORY BUFFER
========================= */

static inline void pushSample(float signal)
{
    signalHistory[head++] = signal;

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

    out += "\"signalData\":[";

    for (int i = 0; i < size; i++)
    {
        const int idx = (start + i) % HISTORY;
        out += String(signalHistory[idx], 1);

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
    pushSample(status.signal);

    String out;
    out.reserve(2048);

    out += "{";

    /* ===== SENSOR ===== */
    out += "\"adc\":" + String(status.adc_raw, 1) + ",";
    out += "\"adc_smooth\":" + String(status.adc_smooth, 3) + ",";
    out += "\"signal\":" + String(status.signal, 3) + ",";
    out += "\"voltage\":" + String(status.adc_voltage, 3) + ",";

    /* ===== WIFI ===== */
    out += "\"wifi_rssi\":" + String(status.wifi_rssi) + ",";
    out += "\"wifi_tx_power\":" + String(status.wifi_tx_power, 2) + ",";
    out += "\"wifi_connected\":" +
           String(status.wifi_connected ? "true" : "false") + ",";

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
<title>BPW34 X-ray Detector</title>
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

<h2>BPW34 X-ray Detector</h2>

<div class="header">
    <div>SIGNAL <span id="signal" class="value">0</span></div>
    <div>V <span id="voltage" class="value" style="color:#ffcc00">0</span></div>
</div>

<div class="status">
ADC: <span id="adc">0</span> |
RSSI: <span id="rssi">0</span> dBm |
TEMP: <span id="temp">0</span> °C |
WIFI: <span id="wifi">-</span> |
ALARM: <span id="alarm">-</span>
</div>

<canvas id="chart"></canvas>

<script>
const ctx = document.getElementById('chart');

const chart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Signal',
            data: [],
            borderColor: 'rgba(0, 229, 255, 0.8)',
            backgroundColor: 'rgba(0, 229, 255, 0.1)',
            borderWidth: 1,
            pointRadius: 0,
            fill: true
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

        document.getElementById('signal').innerText =
            d.signal.toFixed(1);

        document.getElementById('voltage').innerText =
            d.voltage.toFixed(3);

        document.getElementById('adc').innerText =
            d.adc_smooth.toFixed(1);

        document.getElementById('rssi').innerText =
            d.wifi_rssi;

        document.getElementById('temp').innerText =
            d.cpu_temp.toFixed(1);

        document.getElementById('wifi').innerText =
            d.wifi_connected ? "OK" : "NO";

        document.getElementById('alarm').innerText =
            d.alarm ? "YES" : "NO";

        if (d.signalData)
        {
            chart.data.labels = d.signalData.map((_, i) => i);
            chart.data.datasets[0].data = d.signalData;

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