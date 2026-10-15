#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <functional>

using MqttCallback = std::function<void(const String& topic, const String& payload)>;
using MqttConnectCallback = std::function<void()>;

class MQTTManager {
public:
  MQTTManager(WiFiClient& wifi_client);

  void begin(const char* broker, uint16_t port, const char* user, const char* pass, const String& client_id = "");
  void set_timeout(uint32_t ms);
  void loop();
  bool publish(const String& topic, const String& payload, bool retained = false);
  bool subscribe(const String& topic);
  void set_callback(MqttCallback cb);
  void set_connect_callback(MqttConnectCallback cb);
  bool is_connected();

private:
  PubSubClient client_;
  WiFiClient& wifi_client_;
  const char* broker_;
  uint16_t port_;
  const char* user_;
  const char* pass_;
  MqttCallback user_callback_;
  MqttConnectCallback connect_callback_;
  String client_id_;
  uint32_t last_reconnect_attempt_;
  String subscribed_topics_[10];
  int topic_count_;
  static void internal_callback(char* topic, byte* payload, unsigned int length);
  void connect_or_reconnect();
  void resubscribe();
};

#endif