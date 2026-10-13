#include <Arduino.h>
#include <WebServer.h>
#include "AppConfig.h"
#include "Secrets.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "telemetry.h"
#include "ota_manager.h"

static constexpr uint16_t HTTP_PORT = 80;

WiFiManager wifi;
WiFiClient wifi_client;
MQTTManager mqtt(wifi_client);
Telemetry telemetry;
OTAManager ota(OTA_PORT, OTA_PATH, OTA_PASSWORD);
WebServer http_server(HTTP_PORT);

bool led_state = false;
uint32_t last_telemetry_ms = 0;
uint32_t last_log_ms = 0;
String device_topic_base;

void write_led(bool on) {
  led_state = on;
  if (LED_ACTIVE_LOW) {
    digitalWrite(LED_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(LED_PIN, on ? HIGH : LOW);
  }
}

// --- HTTP LED handlers ---

void add_cors_headers() {
  http_server.sendHeader("Access-Control-Allow-Origin", "*");
  http_server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  http_server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handle_led_on() {
  write_led(true);
  add_cors_headers();
  http_server.send(200, "application/json", "{\"led\":\"on\"}");
}

void handle_led_off() {
  write_led(false);
  add_cors_headers();
  http_server.send(200, "application/json", "{\"led\":\"off\"}");
}

void handle_led_status() {
  add_cors_headers();
  String json = String("{\"led\":\"") + (led_state ? "on" : "off") + "\"}";
  http_server.send(200, "application/json", json);
}

void handle_led_options() {
  add_cors_headers();
  http_server.send(204);
}

void handle_http_not_found() {
  add_cors_headers();
  http_server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
}

// --- MQTT ---

void on_mqtt_message(const String& topic, const String& payload) {
  Serial.printf("MQTT message: %s -> %s\n", topic.c_str(), payload.c_str());

  if (topic.endsWith("/cmd/led")) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("JSON parse error: %s\n", err.c_str());
      return;
    }
    const char* cmd = doc["led"];
    if (strcmp(cmd, "on") == 0) {
      write_led(true);
    } else if (strcmp(cmd, "off") == 0) {
      write_led(false);
    }
  }
}

void on_mqtt_connect() {
  Serial.println("MQTT reconnected, publishing status");
  TelemetryData data = telemetry.collect(led_state, true, wifi.is_connected(), wifi.mac(), wifi.local_ip());
  String json = telemetry.to_json(data);
  mqtt.publish(device_topic_base + "/status", json, true);
}

// --- Setup ---

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.printf("Reset reason: %d\n", esp_reset_reason());

  pinMode(LED_PIN, OUTPUT);
  write_led(false);

  wifi.connect(WIFI_SSID, WIFI_PASS);

  device_topic_base = "esp32/" + wifi.mac();

  String client_id = "esp32-";
  client_id += wifi.mac();
  mqtt.begin(MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASS, client_id);
  mqtt.set_callback(on_mqtt_message);
  mqtt.set_connect_callback(on_mqtt_connect);
  mqtt.subscribe(device_topic_base + "/#");

  ota.begin();

  http_server.on("/led/on", HTTP_GET, handle_led_on);
  http_server.on("/led/off", HTTP_GET, handle_led_off);
  http_server.on("/led/status", HTTP_GET, handle_led_status);
  http_server.on("/led/on", HTTP_OPTIONS, handle_led_options);
  http_server.on("/led/off", HTTP_OPTIONS, handle_led_options);
  http_server.on("/led/status", HTTP_OPTIONS, handle_led_options);
  http_server.onNotFound(handle_http_not_found);
  http_server.begin();

  Serial.printf("HTTP server started on port %d\n", HTTP_PORT);
}

void loop() {
  wifi.reconnect();
  mqtt.loop();
  ota.handle();
  http_server.handleClient();

  uint32_t now = millis();
  if (now - last_telemetry_ms >= TELEMETRY_INTERVAL_MS) {
    last_telemetry_ms = now;

    TelemetryData data = telemetry.collect(led_state, mqtt.is_connected(), wifi.is_connected(), wifi.mac(), wifi.local_ip());
    String json = telemetry.to_json(data);

    if (mqtt.is_connected()) {
      mqtt.publish(device_topic_base + "/telemetry", json);
    }

    if (now - last_log_ms >= 60000) {
      last_log_ms = now;
      Serial.printf("Telemetry -> %s/telemetry\n", device_topic_base.c_str());
    }
  }
}