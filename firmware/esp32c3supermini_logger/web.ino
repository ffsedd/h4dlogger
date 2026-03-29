
void startWeb()
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
              {
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
  <div class="card"><div class="stat">CO2: <span id="co2_val">--</span> ppm</div><canvas id="co2Chart" height="180"></canvas></div>
  <div class="card"><div class="stat">Temp: <span id="temp_val">--</span> °C</div><canvas id="tempChart" height="180"></canvas></div>
  <div class="card"><div class="stat">Humidity: <span id="hum_val">--</span> %</div><canvas id="humChart" height="180"></canvas></div>
  <div class="card"><div class="stat">dCO₂: <span id="co2grad_val">--</span> ppm/min</div><canvas id="co2gradChart" height="180"></canvas></div>
  <div class="card"><div class="stat">dT: <span id="tgrad_val">--</span> °C/min</div><canvas id="tgradChart" height="180"></canvas></div>
  <div class="card"><div class="stat">dRH: <span id="hgrad_val">--</span> %/min</div><canvas id="hgradChart" height="180"></canvas></div>
  <div class="card"><div class="stat">Pressure: <span id="pres_val">--</span> hPa</div><canvas id="presChart" height="180"></canvas></div>
  <div class="card"><div class="stat">Light: <span id="lux_val">--</span> lx</div><canvas id="luxChart" height="180"></canvas></div>
  <div class="card"><div class="stat">LD1020 Motion: <span id="ld1020_val">--</span></div><canvas id="ld1020Chart" height="180"></canvas></div>
  <div class="card"><div class="stat">AM312 Motion: <span id="am312_val">--</span></div><canvas id="am312Chart" height="180"></canvas></div>
</div>

<script>
const tempEl = document.getElementById("temp_val");
const humEl = document.getElementById("hum_val");
const presEl = document.getElementById("pres_val");
const luxEl = document.getElementById("lux_val");
const co2El = document.getElementById("co2_val");
const co2gradEl = document.getElementById("co2grad_val");
const tgradEl = document.getElementById("tgrad_val");
const hgradEl = document.getElementById("hgrad_val");
const ld1020El = document.getElementById("ld1020_val");
const am312El = document.getElementById("am312_val");
const deviceEl = document.getElementById("device_id");
const ssidEl = document.getElementById("wifi_ssid");
const sysEl = document.getElementById("sys");

const UPDATE_INTERVAL = 5000;
const HISTORY_DURATION = 60*60*1000;
const MAX_POINTS = HISTORY_DURATION / UPDATE_INTERVAL;
const PER_MINUTE = 60;

function chart(id,color,min,max){
    return new Chart(document.getElementById(id), {
        type:"line",
        data:{ labels:[], datasets:[
            { data:[], borderColor:color, borderWidth:1.5, pointRadius:0, tension:0.15 },
            { data:[], borderColor:"#888", borderWidth:1, pointRadius:0, borderDash:[5,5], label:"zero", fill:false }
        ]},
        options:{ responsive:false, animation:false, plugins:{legend:{display:false}}, scales:{x:{display:false},y:{display:true,min:min,max:max}} }
    });
}

function push(chart,v){
    const d = chart.data.datasets[0].data;
    const z = chart.data.datasets[1].data;
    const l = chart.data.labels;
    d.push(v); z.push(0); l.push("");
    if(d.length>MAX_POINTS){ d.shift(); z.shift(); l.shift(); }
    chart.update("none");
}

// Charts
const tempChart = chart("tempChart","#f00",10,30);
const humChart = chart("humChart","#09f",20,80);
const presChart = chart("presChart","#fc5",900,1100);
const luxChart = chart("luxChart","#fff",0,500);
const co2Chart = chart("co2Chart","#0f0",400,1600);
const co2gradChart = chart("co2gradChart","#afa",-200,100);
const tgradChart = chart("tgradChart","#faa",-5,5);
const hgradChart = chart("hgradChart","#0ef",-10,10);
const ld1020Chart = chart("ld1020Chart","#fa0",0,1);
const am312Chart = chart("am312Chart","#f0f",0,1);

async function update(){
    const r = await fetch("/data");
    const j = await r.json();

    deviceEl.innerText = j.device;
    ssidEl.innerText = j.ssid;
    co2El.innerText = j.co2.toFixed(0);
    tempEl.innerText = j.temp.toFixed(2);
    humEl.innerText = j.hum.toFixed(2);
    presEl.innerText = j.pres.toFixed(1);
    luxEl.innerText = j.lux.toFixed(1);
    co2gradEl.innerText = (j.co2_grad*PER_MINUTE).toFixed(1);
    tgradEl.innerText = (j.temp_grad*PER_MINUTE).toFixed(2);
    hgradEl.innerText = (j.hum_grad*PER_MINUTE).toFixed(2);
    ld1020El.innerText = j.ld1020_motion?"Yes":"No";
    am312El.innerText = j.am312_motion?"Yes":"No";

    push(tempChart, j.temp);
    push(humChart, j.hum);
    push(presChart, j.pres);
    push(luxChart, j.lux);
    push(co2Chart, j.co2);
    push(co2gradChart, j.co2_grad*PER_MINUTE);
    push(tgradChart, j.temp_grad*PER_MINUTE);
    push(hgradChart, j.hum_grad*PER_MINUTE);
    push(ld1020Chart, j.ld1020_motion?1:0);
    push(am312Chart, j.am312_motion?1:0);

    sysEl.innerHTML =
        "WiFi RSSI: "+j.wifi_rssi+" dBm<br>"+
        "Heap: "+j.heap+" B<br>"+
        "Min heap: "+j.heap_min+" B<br>"+
        "Uptime: "+j.uptime+" s<br>"+
        "MQTT: "+j.mqtt;
}

setInterval(update, UPDATE_INTERVAL);
</script>
</body>
</html>
)rawliteral";

        req->send_P(200, "text/html", page); });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req)
              { req->send(200, "application/json", jsonData()); });

    server.begin();
    Serial.println("[WEB] started");
}
