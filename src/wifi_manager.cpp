#include "wifi_manager.h"
#include "AppConfig.h"
#include <esp_task_wdt.h>

void WiFiManager::connect(const char* ssid, const char* password) {
  Serial.printf("Connecting to Wi-Fi %s", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    esp_task_wdt_reset();
    delay(50);
    Serial.print(".");
    if (millis() - start >= WIFI_TIMEOUT_MS) {
      Serial.println("\nWi-Fi connection timed out");
      return;
    }
  }

  Serial.printf("\nWi-Fi connected. IP: %s, MAC: %s\n", local_ip().toString().c_str(), mac().c_str());
}

void WiFiManager::reconnect() {
  if (!is_connected()) {
    WiFi.disconnect(false);
    WiFi.reconnect();
  }
}

bool WiFiManager::is_connected() const {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiManager::local_ip() const {
  return WiFi.localIP();
}

int16_t WiFiManager::rssi() const {
  return WiFi.RSSI();
}

String WiFiManager::mac() const {
  uint8_t raw[6];
  WiFi.macAddress(raw);
  char buf[13];
  snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
           raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
  return String(buf);
}