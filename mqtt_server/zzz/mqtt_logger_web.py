#!/usr/bin/env python3
import sqlite3
import yaml
import time
import threading
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
import paho.mqtt.client as mqtt

DB_FILE = "/mnt/data/db.sqlite"
CONFIG_FILE = "/mnt/data/config.yaml"
WEB_PORT = 8080  # change if needed

# --- Load config ---
with open(CONFIG_FILE) as f:
    cfg = yaml.safe_load(f)

BROKER = cfg["mqtt"]["broker"]
PORT = cfg["mqtt"].get("port", 1883)
KEEPALIVE = cfg["mqtt"].get("keepalive", 60)

# --- Prepare database ---
Path(DB_FILE).parent.mkdir(parents=True, exist_ok=True)
conn = sqlite3.connect(DB_FILE, check_same_thread=False)
cur = conn.cursor()
cur.execute("""
CREATE TABLE IF NOT EXISTS readings (
    ts INTEGER,
    room TEXT,
    sensor_id TEXT,
    metric TEXT,
    value REAL
)
""")
conn.commit()

# --- MQTT callbacks ---
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with code", rc)
    for room, sensors in cfg["sensors"].items():
        for sensor_id, metrics in sensors.items():
            for metric, topic in metrics.items():
                client.subscribe(topic)
                print(f"Subscribed to {topic}")

def on_message(client, userdata, msg):
    for room, sensors in cfg["sensors"].items():
        for sensor_id, metrics in sensors.items():
            for metric, topic in metrics.items():
                if msg.topic == topic:
                    try:
                        value = float(msg.payload.decode())
                    except ValueError:
                        print(f"Invalid payload for {msg.topic}: {msg.payload}")
                        return
                    ts = int(time.time())
                    cur.execute("INSERT INTO readings VALUES (?, ?, ?, ?, ?)",
                                (ts, room, sensor_id, metric, value))
                    conn.commit()
                    print(f"{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(ts))} "
                          f"{room}/{sensor_id}/{metric} = {value}")
                    return

# --- MQTT client ---
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, KEEPALIVE)

# --- Simple web server ---
class RequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != '/':
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        html = "<html><head><title>Sensor Readings</title><meta http-equiv='refresh' content='5'></head><body>"
        html += "<h2>Latest Sensor Readings</h2><table border='1'><tr><th>Room</th><th>Sensor</th><th>Metric</th><th>Value</th><th>Timestamp</th></tr>"
        # Get latest reading per room/sensor/metric
        cur.execute("""
            SELECT room, sensor_id, metric, value, ts
            FROM readings
            WHERE (room, sensor_id, metric, ts) IN (
                SELECT room, sensor_id, metric, MAX(ts) FROM readings GROUP BY room, sensor_id, metric
            )
            ORDER BY room, sensor_id, metric
        """)
        rows = cur.fetchall()
        for r in rows:
            html += f"<tr>{r[0]!r}{r[1]!r}{r[2]!r}{r[3]!r}{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(r[4]))}</tr>"
        html += "</table></body></html>"
        self.wfile.write(html.encode())

def run_server():
    server = HTTPServer(('0.0.0.0', WEB_PORT), RequestHandler)
    print(f"Web server running on port {WEB_PORT}")
    server.serve_forever()

# --- Run everything ---
threading.Thread(target=run_server, daemon=True).start()
client.loop_forever()
