#include "ota_manager.h"
#include <Update.h>

OTAManager::OTAManager(uint16_t port, const char* path, const char* password)
  : port_(port)
  , path_(path)
  , password_(password)
  , server_(port_)
{
}

void OTAManager::begin() {
  server_.on(path_.c_str(), HTTP_GET, [this]() { handle_update_form(); });
  server_.on(path_.c_str(), HTTP_POST, [this]() {
    server_.send(200, "text/plain", "Update complete");
    delay(1000);
    ESP.restart();
  }, [this]() { handle_update_upload(); });

  server_.begin();
  Serial.printf("OTA server started on port %d, path %s\n", port_, path_.c_str());
}

void OTAManager::handle() {
  server_.handleClient();
}

void OTAManager::handle_update_form() {
  if (!authenticate()) return;

  String html = build_form_page();
  server_.send(200, "text/html", html);
}

void OTAManager::handle_update_upload() {
  if (!authenticate()) return;

  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA upload started: %s (%u bytes)\n",
                  upload.filename.c_str(), upload.totalSize);
    if (!Update.begin(upload.totalSize)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA update success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

bool OTAManager::authenticate() {
  if (password_.length() == 0) return true;

  if (!server_.authenticate("admin", password_.c_str())) {
    server_.requestAuthentication();
    return false;
  }
  return true;
}

String OTAManager::build_form_page() {
  return F(
    "<!DOCTYPE html>"
    "<html><head><meta charset='UTF-8'>"
    "<title>ESP32 OTA Update</title>"
    "<style>"
    "body{font-family:Arial;text-align:center;margin-top:60px;background:#f2f2f2}"
    "h1{color:#333}"
    "form{border:2px dashed #aaa;padding:30px;display:inline-block;background:#fff;border-radius:12px}"
    "input[type=file]{margin:15px 0}"
    "input[type=submit]{background:#007bff;color:#fff;border:none;padding:12px 28px;border-radius:6px;cursor:pointer;font-size:16px}"
    "input[type=submit]:hover{opacity:.8}"
    "</style></head><body>"
    "<h1>Firmware Update</h1>"
    "<form method='POST' enctype='multipart/form-data'>"
    "<input type='file' name='firmware' accept='.bin' required><br>"
    "<input type='submit' value='Upload & Update'>"
    "</form></body></html>"
  );
}