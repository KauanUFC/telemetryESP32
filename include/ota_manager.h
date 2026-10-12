#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>

class OTAManager {
public:
  OTAManager(uint16_t port, const char* path, const char* password);
  void begin();
  void handle();

private:
  uint16_t port_;
  String path_;
  String password_;
  WebServer server_;

  void handle_update_form();
  void handle_update_upload();
  bool authenticate();
  String build_form_page();
};

#endif