#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;
ESP32P4_Dsp dsp;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 05_MotionDetect ===");
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }
  dsp.begin(cam.width(), cam.height(), 22);
}

void loop() {
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
