/**
 * 04_WiFiMjpeg — Camera UI over ESP32-C6 Wi-Fi.
 *
 * Camera + C6 SDIO + SSID: board_config.h
 * (docs/Custom-Boards.md). Not Guition-specific.
 *
 * Sketch-owned FB + tiny preview + model JSON: 43_CamWebModels.
 */

#ifndef APP_NAME
#define APP_NAME "04_WiFiMjpeg"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

#include "board_config.h"

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 04_WiFiMjpeg ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
  esp32csi_print_cam_config(cam_cfg);
  if (!cam.begin(cam_cfg)) {
    Serial.println("camera FAILED — ESP32CSI_CAM_*");
    while (true) delay(1000);
  }

  esp32csi_wifi_config_t wifi_cfg = esp32csi_wifi_config();
  esp32csi_print_wifi_config(wifi_cfg);
  if (!esp32csi_wifi_begin(wifi_cfg)) {
    Serial.println("Wi-Fi FAILED — ESP32CSI_WIFI_* / C6 SDIO pins");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 35)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }
  IPAddress ip = esp32csi_wifi_ip();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
}

void loop() {
  stream.loop();
  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("[hb] sent=%u jpeg=%u\n", (unsigned)stream.sent(),
                  (unsigned)stream.lastJpegBytes());
  }
}
