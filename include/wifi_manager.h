#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
  void connect(const char* ssid, const char* password);
  void reconnect();
  bool is_connected() const;
  IPAddress local_ip() const;
  int16_t rssi() const;
  String mac() const;
};

#endif