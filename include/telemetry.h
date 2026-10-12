#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct TelemetryData {
  String device_id;
  String mac;
  String ip;
  uint32_t uptime_s;
  int16_t rssi;
  uint32_t free_heap;
  float temp_cpu;
  bool led_state;
  bool mqtt_connected;
  bool wifi_connected;
};

class Telemetry {
public:
  Telemetry();
  TelemetryData collect(bool current_led_state, bool mqtt_ok, bool wifi_ok, const String& device_mac, const IPAddress& device_ip);
  String to_json(const TelemetryData& data);
};

#endif