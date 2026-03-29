void startWeb() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
body { font-family:system-ui; background:#0e0e0e; color:#eee; margin:16px; }
h1 { margin-bottom:6px; font-size:20px; }
.grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(200px,1fr)); gap:8px; }
.card { background:#1a1a1a; padding:8px; border-radius:8px; }
canvas { width:100%; }
.stat { font-size:16px; margin-bottom:4px; }
.sys { margin-top:8px; font-size:13px; line-height:1.4; }
</style>
</head>
<body>

<h1>ESP32 dlogger</h1>

<div class="card sys">
<b>System info</b><br>
Device: <span id="device_id">--</span><br>
SSID: <span id="wifi_ssid">--</span><br>
<div id="sys"></div>
</div>

<div class="grid">
  <div class="card"><div class="stat">Temp: <span id="temp_val">--</span> °C</div><canvas id="tempChart" height="180"></canvas></div>
  <div class="card"><div class="stat">Humidity: <span id="hum_val">--</span> %</div><canvas id="humChart" height="180"></canvas></div>
</div>

<script>
const tempEl = document.getElementById("temp_val");
const humEl = document.getElementById("hum_val");
const deviceEl = document.getElementById("device_id");
const ssidEl = document.getElementById("wifi_ssid");
const sysEl = document.getElementById("sys");

const UPDATE_INTERVAL = 5000;
const HISTORY_DURATION = 60*60*1000;
const MAX_POINTS = HISTORY_DURATION / UPDATE_INTERVAL;

function chart(id,color,min,max){
    return new Chart(document.getElementById(id), {
        type:"line",
        data:{ labels:[], datasets:[{ data:[], borderColor:color, borderWidth:1.5, pointRadius:0, tension:0.15 }]},
        options:{ responsive:false, animation:false, plugins:{legend:{display:false}}, scales:{x:{display:false},y:{min:min,max:max}} }
    });
}

function push(chart,v){
    const d = chart.data.datasets[0].data;
    const l = chart.data.labels;
    d.push(v); l.push("");
    if(d.length>MAX_POINTS){ d.shift(); l.shift(); }
    chart.update("none");
}

const tempChart = chart("tempChart","#f00",10,30);
const humChart = chart("humChart","#09f",20,80);

async function update(){
    const r = await fetch("/data");
    const j = await r.json();

    deviceEl.innerText = j.device;
    ssidEl.innerText = j.ssid;
    tempEl.innerText = j.temp.toFixed(2);
    humEl.innerText = j.hum.toFixed(2);

    push(tempChart, j.temp);
    push(humChart, j.hum);

    sysEl.innerHTML =
        "WiFi RSSI: "+j.wifi_rssi+" dBm<br>"+
        "Heap: "+j.heap+" B<br>"+
        "Min heap: "+j.heap_min+" B<br>"+
        "Uptime: "+j.uptime+" s";
}

setInterval(update, UPDATE_INTERVAL);
</script>
</body>
</html>
)rawliteral";

        req->send_P(200, "text/html", page);
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(200, "application/json", jsonData());
    });

    server.begin();
    Serial.println("[WEB] started");
}

