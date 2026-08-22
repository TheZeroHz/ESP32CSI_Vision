#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "05_MotionDetect"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif


ESP32P4_Camera cam;
ESP32P4_Dsp dsp;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 05_MotionDetect ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }
  dsp.begin(cam.width(), cam.height(), 22);
}

void loop() {
  dbg.poll();
  camera_fb_t *fb = cam.capture();
  if (!fb) return;
  esp32p4_motion_t m{};
  dsp.detect(fb, &m);
  if (m.moving) {
    Serial.printf("MOTION changed=%u/%u roi=%d,%d %dx%d\n", (unsigned)m.changed, (unsigned)m.total,
                  m.roi.x, m.roi.y, m.roi.w, m.roi.h);
  }
  cam.release(fb);
}
