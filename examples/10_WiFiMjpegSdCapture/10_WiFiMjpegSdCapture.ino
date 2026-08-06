/**
 * 10_WiFiMjpegSdCapture — live MJPEG UI + save JPEG to SD /IMG
 *
 * Based on 04_WiFiMjpeg. Uses ESP32CSI_Vision:
 *   ESP32P4_Camera + ESP32P4_Sd + ESP32P4_MjpegServer
 *
 * Web UI: open http://<ip>/  → press "Capture Img"
 * Files:  /IMG/IMG_00001.jpg , IMG_00002.jpg , …
 *
 * Serial @ 115200 · FAT32 microSD · PSRAM enabled
 */

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

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_MjpegServer stream;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 10_WiFiMjpegSdCapture ===");

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!sd.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("SD FAILED — insert FAT32 card and reset");
    while (true) delay(1000);
  }
  sd.listDir("/", 0);

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

  // Saves under /IMG on the SD card when UI "Capture Img" is pressed
  if (!stream.enableSdCapture(&sd, "/IMG")) {
    Serial.println("enableSdCapture FAILED");
    while (true) delay(1000);
  }

  IPAddress ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Press Capture Img in the UI to save JPEG → /IMG/");
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
