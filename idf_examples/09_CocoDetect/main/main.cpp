/**
 * ESP32-P4 + OV5647 CSI → ESP-DL COCODetect (Arduino-style setup/loop).
 *
 * Requires ESP-IDF 5.4 or 5.5 (not Arduino IDE). From this folder:
 *
 *   idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
 *   idf.py -p COM8 build flash monitor
 *
 * Models on SD (default sdkconfig):
 *   /sdcard/models/p4/coco_detect_yolo11n_320_s8_v1.espdl
 * Copy from library models/espdl/p4/.
 */

#include <Arduino.h>

#include "ESP32CSI_Vision.h"
#include "detect/ESP32P4_ObjectDetect.h"

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_ObjectDetect det;

static esp32p4_det_t g_dets[16];

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== 09_CocoDetect (ESP-IDF + Arduino + ESP-DL) ===");

  if (!sd.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("SD begin FAILED — put models under /models/p4/");
    while (true) {
      delay(1000);
    }
  }
  esp32p4_set_model_mount_point("/sdcard");

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera begin FAILED");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("camera %ux%u\n", cam.width(), cam.height());

  // Prefer YOLO11n 320 for faster serial demo; switch to COCO_YOLO11N for 640.
  if (!det.begin(ESP32P4_ObjectDetect::COCO_YOLO11N_320)) {
    Serial.println("det.begin FAILED (missing coco_detect_yolo11n_320_s8_v1.espdl on SD?)");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("COCODetect ready");
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) {
    Serial.println("capture timeout");
    return;
  }

  uint32_t t0 = millis();
  int n = det.detect(fb, g_dets, 16);
  uint32_t dt = millis() - t0;

  Serial.printf("objs=%d  %ums  %ux%u\n", n, (unsigned)dt, fb->width, fb->height);
  for (int i = 0; i < n; i++) {
    const esp32p4_det_t &d = g_dets[i];
    Serial.printf("  [%d] %s score=%.3f box=%d,%d %dx%d\n", i, det.label(d.category), d.score, d.x,
                  d.y, d.w, d.h);
  }

  cam.release(fb);
  delay(50);
}
