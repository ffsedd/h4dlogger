/* =========================
   ESP32 Datalogger Dashboard
   ========================= */

/* ---------- DOM ---------- */

const $ = id => document.getElementById(id);

const el = {
    temp: $("temp_val"),
    hum: $("hum_val"),
    pres: $("pres_val"),
    lux: $("lux_val"),
    co2: $("co2_val"),

    co2grad: $("co2grad_val"),
    tgrad: $("tgrad_val"),
    hgrad: $("hgrad_val"),

    ld1020: $("ld1020_val"),
    am312: $("am312_val"),

    device: $("device_id"),
    ssid: $("wifi_ssid"),
    sys: $("sys")
};

/* ---------- Config ---------- */

const UPDATE_INTERVAL = 5000;
const HISTORY_DURATION = 60 * 60 * 1000;
const MAX_POINTS = HISTORY_DURATION / UPDATE_INTERVAL;
const PER_MINUTE = 60;

/* ---------- Chart Factory ---------- */

function makeChart(id, color, min, max) {
    return new Chart($(id), {
        type: "line",
        data: {
            labels: [],
            datasets: [
                {
                    data: [],
                    borderColor: color,
                    borderWidth: 1.5,
                    pointRadius: 0,
                    tension: 0.15
                },
                {
                    data: [],
                    borderColor: "#888",
                    borderWidth: 1,
                    pointRadius: 0,
                    borderDash: [5, 5]
                }
            ]
        },
        options: {
            responsive: false,
            animation: false,
            plugins: { legend: { display: false } },
            scales: {
                x: { display: false },
                y: { display: true, min, max }
            }
        }
    });
}

function push(chart, value) {
    const d = chart.data.datasets[0].data;
    const z = chart.data.datasets[1].data;
    const l = chart.data.labels;

    d.push(value);
    z.push(0);
    l.push("");

    if (d.length > MAX_POINTS) {
        d.shift();
        z.shift();
        l.shift();
    }

    chart.update("none");
}

/* ---------- Charts ---------- */

const charts = {
    temp: makeChart("tempChart", "#f00", 10, 30),
    hum: makeChart("humChart", "#09f", 20, 80),
    pres: makeChart("presChart", "#fc5", 900, 1100),
    lux: makeChart("luxChart", "#fff", 0, 500),
    co2: makeChart("co2Chart", "#0f0", 400, 1600),

    co2grad: makeChart("co2gradChart", "#afa", -200, 100),
    tgrad: makeChart("tgradChart", "#faa", -5, 5),
    hgrad: makeChart("hgradChart", "#0ef", -10, 10),

    ld1020: makeChart("ld1020Chart", "#fa0", 0, 1),
    am312: makeChart("am312Chart", "#f0f", 0, 1)
};

/* ---------- Data Update ---------- */

async function update() {
    try {
        const r = await fetch("/data");
        const j = await r.json();

        el.device.textContent = j.device;
        el.ssid.textContent = j.ssid;

        el.co2.textContent = j.co2.toFixed(0);
        el.temp.textContent = j.temp.toFixed(2);
        el.hum.textContent = j.hum.toFixed(2);
        el.pres.textContent = j.pres.toFixed(1);
        el.lux.textContent = j.lux.toFixed(1);

        const co2g = j.co2_grad * PER_MINUTE;
        const tg = j.temp_grad * PER_MINUTE;
        const hg = j.hum_grad * PER_MINUTE;

        el.co2grad.textContent = co2g.toFixed(1);
        el.tgrad.textContent = tg.toFixed(2);
        el.hgrad.textContent = hg.toFixed(2);

        el.ld1020.textContent = j.ld1020_motion ? "Yes" : "No";
        el.am312.textContent = j.am312_motion ? "Yes" : "No";

        push(charts.temp, j.temp);
        push(charts.hum, j.hum);
        push(charts.pres, j.pres);
        push(charts.lux, j.lux);
        push(charts.co2, j.co2);

        push(charts.co2grad, co2g);
        push(charts.tgrad, tg);
        push(charts.hgrad, hg);

        push(charts.ld1020, j.ld1020_motion ? 1 : 0);
        push(charts.am312, j.am312_motion ? 1 : 0);

        el.sys.innerHTML =
            "WiFi RSSI: " + j.wifi_rssi + " dBm<br>" +
            "Heap: " + j.heap + " B<br>" +
            "Min heap: " + j.heap_min + " B<br>" +
            "Uptime: " + j.uptime + " s<br>" +
            "MQTT: " + j.mqtt;

    } catch (err) {
        console.error("update failed", err);
    }
}

/* ---------- Start ---------- */

setInterval(update, UPDATE_INTERVAL);
update();