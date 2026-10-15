#include "mqtt_manager.h"
#include "AppConfig.h"

MQTTManager* instance = nullptr;

MQTTManager::MQTTManager(WiFiClient& wifi_client)
  : client_(wifi_client)
  , wifi_client_(wifi_client)
  , broker_(nullptr)
  , port_(0)
  , user_(nullptr)
  , pass_(nullptr)
  , last_reconnect_attempt_(0)
  , topic_count_(0)
{
  instance = this;
  client_.setCallback(internal_callback);
}

void MQTTManager::begin(const char* broker, uint16_t port, const char* user, const char* pass, const String& client_id) {
  broker_ = broker;
  port_ = port;
  user_ = user;
  pass_ = pass;
  client_id_ = client_id.length() > 0 ? client_id : ("esp32-" + String(random(0xffff), HEX));
  client_.setServer(broker_, port_);
  connect_or_reconnect();
}

void MQTTManager::loop() {
  if (!client_.connected()) {
    connect_or_reconnect();
  }
  client_.loop();
}

void MQTTManager::set_timeout(uint32_t ms) {
  wifi_client_.setTimeout(ms);
}

bool MQTTManager::publish(const String& topic, const String& payload, bool retained) {
  return client_.publish(topic.c_str(), payload.c_str(), retained);
}

bool MQTTManager::subscribe(const String& topic) {
  if (topic_count_ < 10) {
    subscribed_topics_[topic_count_++] = topic;
  }
  return client_.subscribe(topic.c_str());
}

void MQTTManager::set_callback(MqttCallback cb) {
  user_callback_ = cb;
}

void MQTTManager::set_connect_callback(MqttConnectCallback cb) {
  connect_callback_ = cb;
}

bool MQTTManager::is_connected() {
  return client_.connected();
}

void MQTTManager::resubscribe() {
  for (int i = 0; i < topic_count_; i++) {
    client_.subscribe(subscribed_topics_[i].c_str());
  }
}

void MQTTManager::connect_or_reconnect() {
  if (millis() - last_reconnect_attempt_ < MQTT_RECONNECT_INTERVAL_MS) {
    return;
  }
  last_reconnect_attempt_ = millis();

  String cid = client_id_.length() > 0 ? client_id_ : ("esp32-" + String(random(0xffff), HEX));
  Serial.printf("MQTT connecting to %s:%d as %s ... ", broker_, port_, cid.c_str());

  wifi_client_.setTimeout(3000);
  bool ok;
  if (strlen(user_) > 0) {
    ok = client_.connect(cid.c_str(), user_, pass_);
  } else {
    ok = client_.connect(cid.c_str());
  }

  if (ok) {
    Serial.println("connected");
    last_reconnect_attempt_ = 0;
    resubscribe();
    if (connect_callback_) {
      connect_callback_();
    }
  } else {
    Serial.printf("failed (rc=%d)\n", client_.state());
  }
}

void MQTTManager::internal_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  if (instance && instance->user_callback_) {
    instance->user_callback_(String(topic), msg);
  }
}