/**
 * 10_WiFiMjpegSdCapture — live MJPEG UI + save JPEG to storage /IMG
 *
 * Based on 04_WiFiMjpeg. Uses ESP32CSI_Vision:
 *   ESP32P4_Camera + ESP32P4_StoragePref + ESP32P4_MjpegServer
 *
 * Storage preference (edit APP_STORAGE):
 *   ESP32P4_STORAGE_AUTO / SD / FFAT / LITTLEFS / SPIFFS
 *
 * Web UI: open http://<ip>/  → press "Capture Img"
 * Files:  /IMG/IMG_00001.jpg , IMG_00002.jpg , …
 *
 * Serial @ 115200 · PSRAM enabled · FAT partition if using FFat
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif
#ifndef APP_NAME
#define APP_NAME "10_WiFiMjpegSdCapture"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

#include <WiFi.h>
#include "board_config.h"

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_MjpegServer stream;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 10_WiFiMjpegSdCapture ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("APP_STORAGE pref=%s\n", store.kindName(APP_STORAGE));

  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage FAILED - insert SD or use a FAT/LittleFS partition");
    while (true) delay(1000);
  }
  Serial.printf("Storage %s vfs=%s\n", store.label(), store.vfsRoot());

  if (!esp32csi_wifi_begin()) {
    Serial.println("Wi-Fi FAILED");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 35)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }

  // Saves under /IMG on the preferred volume when UI "Capture Img" is pressed
  if (!stream.enableCapture(&store.fs(), "/IMG")) {
    Serial.println("enableCapture FAILED");
    while (true) delay(1000);
  }

  IPAddress ip = esp32csi_wifi_ip();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Press Capture Img in the UI to save JPEG -> /IMG/");
}

void loop() {
  stream.loop();
  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("[hb] sent=%u jpeg=%u saved=%u last=%s\n", (unsigned)stream.sent(),
                  (unsigned)stream.lastJpegBytes(), (unsigned)stream.savedCount(),
                  stream.lastSavedPath());
  }
}
