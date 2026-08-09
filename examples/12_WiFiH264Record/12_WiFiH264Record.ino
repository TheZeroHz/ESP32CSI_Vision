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
#include <ESP32CSI_Vision.h>

// Edit Wi‑Fi credentials:
static const char *WIFI_SSID = "iHUB";
static const char *WIFI_PASS = "iHUB@2026%";

// Guition JC-ESP32P4-M3 C6 SDIO pins (Wi‑Fi — separate from microSD):
static const int C6_SDIO_CLK = 18;
static const int C6_SDIO_CMD = 19;
static const int C6_SDIO_D0 = 14;
static const int C6_SDIO_D1 = 15;
static const int C6_SDIO_D2 = 16;
static const int C6_SDIO_D3 = 17;
static const int C6_SDIO_RST = 54;

static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_H264 h264;
ESP32P4_MjpegServer stream;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 12_WiFiH264Record ===");
  Serial.printf("APP_STORAGE pref=%s\n", ESP32P4_StoragePref::kindName(APP_STORAGE));

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!store.begin(APP_STORAGE, false, &sd, ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("Storage FAILED — insert SD or use a flash FAT/LittleFS partition");
    while (true) delay(1000);
  }
  Serial.printf("Storage %s vfs=%s\n", store.label(), store.vfsRoot());

  // fps = encoder rate-control hint only (not file timing)
  if (!h264.begin(ENC_W, ENC_H, 30, ENC_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  WiFi.setPins(C6_SDIO_CLK, C6_SDIO_CMD, C6_SDIO_D0, C6_SDIO_D1, C6_SDIO_D2, C6_SDIO_D3,
               C6_SDIO_RST);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname("esp32p4-cam");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-P4-Cam", "camstream1");
    Serial.print("SoftAP ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print("WiFi ");
    Serial.println(WiFi.localIP());
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

  IPAddress ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Open UI → Record / Stop  →  /VIDEO/*.mp4 only");
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
