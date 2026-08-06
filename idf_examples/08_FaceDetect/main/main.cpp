/**
 * ESP32-P4 + OV5647 CSI -> ESP-DL HumanFaceDetect (Arduino-style setup/loop).
 *
 * Requires ESP-IDF 5.4 or 5.5 (not Arduino IDE). From this folder:
 *
 *   idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
 *   idf.py -p COM8 build flash monitor
 *
 * Guition JC-ESP32P4-M3: USBMode/CDC via CH340; Serial 115200.
 */

#include <Arduino.h>

#include "ESP32CSI_Vision.h"
#include "ESP32P4_FaceDetect.h"

ESP32P4_Camera cam;
ESP32P4_FaceDetect face;

static esp32p4_face_t g_faces[8];

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== 08_FaceDetect (ESP-IDF + Arduino + ESP-DL) ===");

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera begin FAILED");
    while (true) {
      delay(1000);
    }
  }
  Serial.printf("camera %ux%u\n", cam.width(), cam.height());

  if (!face.begin(ESP32P4_FaceDetect::MSRMNP_S8_V1)) {
    Serial.println("face.begin FAILED (is human_face_detect linked?)");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("HumanFaceDetect ready");
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) {
    Serial.println("capture timeout");
    return;
  }

  uint32_t t0 = millis();
  int n = face.detect(fb, g_faces, 8);
  uint32_t dt = millis() - t0;

  Serial.printf("faces=%d  %ums  %ux%u\n", n, (unsigned)dt, fb->width, fb->height);
  for (int i = 0; i < n; i++) {
    const esp32p4_face_t &f = g_faces[i];
    Serial.printf("  [%d] score=%.3f box=%d,%d %dx%d", i, f.score, f.x, f.y, f.w, f.h);
    if (f.has_landmarks) {
      Serial.printf("  eyeL=(%d,%d) nose=(%d,%d) eyeR=(%d,%d)", f.landmarks[0][0], f.landmarks[0][1],
                    f.landmarks[2][0], f.landmarks[2][1], f.landmarks[3][0], f.landmarks[3][1]);
    }
    Serial.println();
  }

  cam.release(fb);
  delay(50);
}
