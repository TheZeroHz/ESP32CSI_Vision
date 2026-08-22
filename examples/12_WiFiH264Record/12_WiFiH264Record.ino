/**
 * 12_WiFiH264Record — live preview + phone-style Record / Stop → .mp4
 *
 * Like a phone camera app:
 *   - Live MJPEG preview in the browser
 *   - Tap Record to start, Stop to finish
 *   - Only .mp4 is saved under /VIDEO/VID_XXXXX.mp4
 *   - Duration = wall-clock; encodes as fast as the pipeline allows (no fixed fps)
 *
 * APP_STORAGE: AUTO / SD / FFAT / LITTLEFS / SPIFFS
 *
 * Serial @ 115200 · PSRAM enabled
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <WiFi.h>
#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "12_WiFiH264Record"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE | ESP32P4_DBG_H264
#endif

static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_H264 h264;
ESP32P4_MjpegServer stream;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 12_WiFiH264Record ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("APP_STORAGE pref=%s\n", store.kindName(APP_STORAGE));

  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage FAILED - insert SD or use a flash FAT/LittleFS partition");
    while (true) delay(1000);
  }
  Serial.printf("Storage %s vfs=%s\n", store.label(), store.vfsRoot());

  // fps = encoder rate-control hint only (not file timing)
  if (!h264.begin(ENC_W, ENC_H, 30, ENC_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  if (!esp32csi_wifi_begin()) {
    Serial.println("Wi-Fi FAILED");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 35)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }

  // Optional stills + video record (phone UI)
  stream.enableCapture(&store.fs(), "/IMG");
  if (!stream.enableVideoRecord(&store.fs(), &h264, "/VIDEO")) {
    Serial.println("enableVideoRecord FAILED");
    while (true) delay(1000);
  }

  IPAddress ip = esp32csi_wifi_ip();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Open UI -> Record / Stop -> /VIDEO/*.mp4 only");
}

void loop() {
  stream.loop();
  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("[hb] sent=%u  recording=%u  videos=%u  last=%s\n", (unsigned)stream.sent(),
                  stream.isRecording() ? 1u : 0u, (unsigned)stream.videosSaved(),
                  stream.lastVideoPath());
  }
}
