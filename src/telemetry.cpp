#include "telemetry.h"
#include <WiFi.h>

static String _device_id;

Telemetry::Telemetry() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[13];
  snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  _device_id = String("esp32-") + buf;
}

TelemetryData Telemetry::collect(bool current_led_state, bool mqtt_ok, bool wifi_ok, const String& device_mac, const IPAddress& device_ip) {
  TelemetryData d;
  d.device_id = _device_id;
  d.mac = device_mac;
  d.ip = device_ip.toString();
  d.uptime_s = millis() / 1000;
  d.rssi = WiFi.RSSI();
  d.free_heap = ESP.getFreeHeap();
  d.led_state = current_led_state;
  d.mqtt_connected = mqtt_ok;
  d.wifi_connected = wifi_ok;
  d.temp_cpu = temperatureRead();
  return d;
}

String Telemetry::to_json(const TelemetryData& d) {
  JsonDocument doc;

  doc["device_id"] = d.device_id;
  doc["mac"] = d.mac;
  doc["ip"] = d.ip;
  doc["uptime_s"] = d.uptime_s;
  doc["rssi"] = d.rssi;
  doc["free_heap"] = d.free_heap;
  doc["temp_cpu"] = d.temp_cpu;
  doc["led_state"] = d.led_state ? "on" : "off";
  doc["mqtt_connected"] = d.mqtt_connected;
  doc["wifi_connected"] = d.wifi_connected;

  String output;
  serializeJson(doc, output);
  return output;
}