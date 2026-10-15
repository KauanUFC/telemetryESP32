const express = require("express");
const mqtt = require("mqtt");
const multer = require("multer");

const app = express();
const PORT = 3000;
const ESP32_OTA_PORT = 8266;
const ESP32_OTA_USER = process.env.ESP32_OTA_USER || "admin";
const ESP32_OTA_PASS = process.env.ESP32_OTA_PASS || "123";
const MQTT_BROKER = process.env.MQTT_BROKER || "mqtt://<ip>:1883";
// fallback WSS para HiveMQ (usar via env var se necessario)
// MQTT_BROKER=wss://broker.hivemq.com:8884/mqtt node app.js

const upload = multer({ storage: multer.memoryStorage(), limits: { fileSize: 4 * 1024 * 1024 } });

// --- Device state (discovered via telemetry) ---
let commandMode = "http";
let latestTelemetry = {};
let deviceInfo = { ip: null, mac: null, deviceId: null };
let sseClients = [];
let mqttConnected = false;
let espHttpOnline = false;

// --- MQTT ---
const mqttClient = mqtt.connect(MQTT_BROKER);

mqttClient.on("connect", () => {
  mqttConnected = true;
  console.log("MQTT connected to broker");
  mqttClient.subscribe("esp32/+/telemetry");
  mqttClient.subscribe("esp32/+/status");
});

mqttClient.on("error", (err) => {
  console.error("MQTT error:", err.message);
});

mqttClient.on("offline", () => {
  mqttConnected = false;
  console.log("MQTT offline");
});

mqttClient.on("reconnect", () => {
  console.log("MQTT reconnecting...");
});

mqttClient.on("message", (topic, message) => {
  try {
    const payload = JSON.parse(message.toString());
    const deviceId = topic.split("/")[1] || payload.device_id;
    latestTelemetry[deviceId] = payload;
    latestTelemetry[deviceId]._timestamp = Date.now();

    // Track device info from telemetry
    if (payload.ip && payload.mac) {
      deviceInfo.ip = payload.ip;
      deviceInfo.mac = payload.mac;
      deviceInfo.deviceId = deviceId;
      console.log(`Device discovered: ${deviceId} @ ${payload.ip} (MAC: ${payload.mac})`);
    }

    sseClients.forEach(res => {
      res.write(`data: ${JSON.stringify(payload)}\n\n`);
    });
  } catch (e) {
    console.error("MQTT parse error:", e.message);
  }
});

// --- Middleware ---
app.use(express.static("public"));

// --- Helpers ---
function espIp() {
  return deviceInfo.ip;
}

function espMac() {
  return deviceInfo.mac;
}

function espDeviceId() {
  return deviceInfo.deviceId;
}

// --- REST: LED control ---
async function sendHttp(action) {
  const endpoint = action === "on" ? "on" : "off";
  const response = await fetch(`http://${espIp()}/led/${endpoint}`);
  return response.json();
}

function sendMqtt(action) {
  const topic = `esp32/${espMac()}/cmd/led`;
  mqttClient.publish(topic, JSON.stringify({ led: action }));
  return { led: action, mode: "mqtt" };
}

app.get("/api/led/on", async (req, res) => {
  try {
    const data = commandMode === "mqtt" ? sendMqtt("on") : await sendHttp("on");
    res.json(data);
  } catch (error) {
    res.status(500).json({ error: "Could not send command", details: error.message });
  }
});

app.get("/api/led/off", async (req, res) => {
  try {
    const data = commandMode === "mqtt" ? sendMqtt("off") : await sendHttp("off");
    res.json(data);
  } catch (error) {
    res.status(500).json({ error: "Could not send command", details: error.message });
  }
});

app.get("/api/led/status", async (req, res) => {
  try {
    const response = await fetch(`http://${espIp()}/led/status`);
    const data = await response.json();
    res.json(data);
  } catch (error) {
    res.status(500).json({ error: "Could not connect to ESP32", details: error.message });
  }
});

// --- HTTP health check ---
setInterval(async () => {
  if (!deviceInfo.ip) {
    espHttpOnline = false;
    return;
  }
  try {
    const res = await fetch(`http://${deviceInfo.ip}/led/status`, { signal: AbortSignal.timeout(3000) });
    espHttpOnline = res.ok;
  } catch {
    espHttpOnline = false;
  }
}, 10000);

// --- Config endpoints ---
app.get("/api/config", (req, res) => {
  res.json({ commandMode, device: deviceInfo, mqtt_connected: mqttConnected, esp_http_online: espHttpOnline });
});

app.post("/api/config/mode", express.json(), (req, res) => {
  const { mode } = req.body;
  if (mode !== "http" && mode !== "mqtt") {
    return res.status(400).json({ error: "Mode must be 'http' or 'mqtt'" });
  }
  commandMode = mode;
  console.log(`Command mode switched to: ${mode}`);
  res.json({ commandMode });
});

// --- OTA upload ---
app.post("/api/ota/upload", upload.single("firmware"), async (req, res) => {
  if (!req.file) {
    return res.status(400).json({ error: "No firmware file provided" });
  }

  const ext = req.file.originalname.toLowerCase().slice(-4);
  if (ext !== ".bin") {
    return res.status(400).json({ error: "File must be a .bin firmware" });
  }

  const otaUrl = `http://${espIp()}:${ESP32_OTA_PORT}/update`;
  const auth = Buffer.from(`${ESP32_OTA_USER}:${ESP32_OTA_PASS}`).toString("base64");
  const boundary = "----OTAFormBoundary" + Date.now();

  const bodyParts = [];
  bodyParts.push(Buffer.from(
    `--${boundary}\r\nContent-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\nContent-Type: application/octet-stream\r\n\r\n`
  ));
  bodyParts.push(req.file.buffer);
  bodyParts.push(Buffer.from(`\r\n--${boundary}--\r\n`));
  const body = Buffer.concat(bodyParts);

  try {
    const response = await fetch(otaUrl, {
      method: "POST",
      headers: {
        "Content-Type": `multipart/form-data; boundary=${boundary}`,
        "Content-Length": body.length.toString(),
        Authorization: `Basic ${auth}`,
      },
      body,
      signal: AbortSignal.timeout(60000),
    });

    const text = await response.text();
    const ok = response.ok;
    console.log(`OTA response (${response.status}): ${text}`);
    res.json({ success: ok, status: response.status, message: text.trim() });
  } catch (error) {
    console.error("OTA upload error:", error.message);
    res.status(502).json({ error: "OTA upload failed", details: error.message });
  }
});

// --- SSE endpoint ---
app.get("/events", (req, res) => {
  res.writeHead(200, {
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-cache",
    Connection: "keep-alive",
    "Access-Control-Allow-Origin": "*",
  });

  Object.values(latestTelemetry).forEach(data => {
    res.write(`data: ${JSON.stringify(data)}\n\n`);
  });

  sseClients.push(res);
  console.log(`SSE client connected (total: ${sseClients.length})`);

  req.on("close", () => {
    sseClients = sseClients.filter(c => c !== res);
    console.log(`SSE client disconnected (total: ${sseClients.length})`);
  });
});

// --- Start ---
app.listen(PORT, () => {
  console.log(`Web app running at http://localhost:${PORT}`);
});