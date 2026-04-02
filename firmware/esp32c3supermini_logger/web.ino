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
  <div class="card"><div class="stat">CO2: <span id="co2_val">--</span> ppm</div><canvas id="co2Chart" height="180"></canvas></div>
  <div class="card"><div class="stat">Temp: <span id="temp_val">--</span> °C</div><canvas id="tempChart" height="180"></canvas></div>
  <div class="card"><div class="stat">Humidity: <span id="hum_val">--</span> %</div><canvas id="humChart" height="180"></canvas></div>
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
const ld1020El = document.getElementById("ld1020_val");
const am312El = document.getElementById("am312_val");
const deviceEl = document.getElementById("device_id");
const ssidEl = document.getElementById("wifi_ssid");
const sysEl = document.getElementById("sys");

const UPDATE_INTERVAL = 5000;
const HISTORY_DURATION = 60*60*1000;
const MAX_POINTS = HISTORY_DURATION / UPDATE_INTERVAL;
const PER_MINUTE = 60;

function subplot(id,color,vmin,vmax,vbase,gmin,gmax,gbase){
    const c = new Chart(document.getElementById(id),{
        type:"line",
        data:{
            labels:[],
            datasets:[
                { data:[], borderColor:color, borderWidth:1.6, pointRadius:0, tension:0.15, yAxisID:"y" },
                { data:[], borderColor:"#555", borderDash:[5,5], pointRadius:0, borderWidth:1, yAxisID:"y" },
                { data:[], borderColor:"#333", borderWidth:1.4, pointRadius:0, tension:0.15, yAxisID:"y2" },
                { data:[], borderColor:"#222", borderDash:[5,5], pointRadius:0, borderWidth:1, yAxisID:"y2" }
            ]
        },
        options:{
            responsive:false,
            animation:false,
            plugins:{legend:{display:false}},
            scales:{
                x:{display:false},
                y:{position:"left",min:vmin,max:vmax,weight:2},
                y2:{position:"right",min:gmin,max:gmax,grid:{drawOnChartArea:false},offset:true,weight:1}
            }
        }
    });
    c.vbase=vbase; c.gbase=gbase;
    return c;
}

function pushSub(chart,value,grad){
    const l=chart.data.labels;
    const v=chart.data.datasets[0].data; const vb=chart.data.datasets[1].data;
    const g=chart.data.datasets[2].data; const gb=chart.data.datasets[3].data;
    v.push(value); vb.push(chart.vbase);
    g.push(grad); gb.push(chart.gbase);
    l.push("");
    if(v.length>MAX_POINTS){ v.shift(); vb.shift(); g.shift(); gb.shift(); l.shift(); }
    chart.update("none");
}

// Charts
const tempChart = subplot("tempChart","#f00",10,30,20,-1,1,0);
const humChart = subplot("humChart","#09f",20,80,50,-2,2,0);
const presChart = subplot("presChart","#fc5",900,1100,1000,-1,1,0);
const luxChart = subplot("luxChart","#fff",0,500,100,0,0,0);
const co2Chart = subplot("co2Chart","#0f0",400,2000,1000,-150,50,0);
const ld1020Chart = subplot("ld1020Chart","#fa0",0,1,0,0,0,0);
const am312Chart = subplot("am312Chart","#f0f",0,1,0,0,0,0);

async function update(){
    const r = await fetch("/data");
    const j = await r.json();

    deviceEl.innerText=j.device;
    ssidEl.innerText=j.ssid;
    co2El.innerText=j.co2.toFixed(0);
    tempEl.innerText=j.temp.toFixed(2);
    humEl.innerText=j.hum.toFixed(2);
    presEl.innerText=j.pres.toFixed(1);
    luxEl.innerText=j.lux.toFixed(1);
    ld1020El.innerText=j.ld1020_motion?"Yes":"No";
    am312El.innerText=j.am312_motion?"Yes":"No";

    pushSub(tempChart,j.temp,j.temp_grad*PER_MINUTE);
    pushSub(humChart,j.hum,j.hum_grad*PER_MINUTE);
    pushSub(presChart,j.pres,j.pres_grad ? j.pres_grad*PER_MINUTE : 0);
    pushSub(luxChart,j.lux,0);
    pushSub(co2Chart,j.co2,j.co2_grad*PER_MINUTE);
    pushSub(ld1020Chart,j.ld1020_motion?1:0,0);
    pushSub(am312Chart,j.am312_motion?1:0,0);

    sysEl.innerHTML =
        "WiFi RSSI: "+j.wifi_rssi+" dBm<br>"+
        "Heap: "+j.heap+" B<br>"+
        "Min heap: "+j.heap_min+" B<br>"+
        "Uptime: "+j.uptime+" s<br>"+
        "MQTT: "+j.mqtt;
}

setInterval(update,UPDATE_INTERVAL);
</script>
</body>
</html>
)rawliteral";

        req->send_P(200, "text/html", page);
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", jsonData());
    });

    server.begin();
    Serial.println("[WEB] started");
}
