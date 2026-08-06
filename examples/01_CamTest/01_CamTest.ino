#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 01_CamTest ===");
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera begin FAILED");
    while (true) delay(1000);
  }
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) {
    Serial.println("capture timeout");
    return;
  }
  uint32_t sum = 0;
  size_t n = fb->len / 2;
  const uint16_t *p = (const uint16_t *)fb->buf;
  size_t step = n > 2048 ? n / 2048 : 1;
  size_t samples = 0;
  for (size_t i = 0; i < n; i += step) {
    sum += p[i];
    samples++;
  }
  Serial.printf("%ux%u mean=%u psram_free=%u\n", fb->width, fb->height,
                (unsigned)(samples ? sum / samples : 0), (unsigned)esp32p4_psram_free_size());
  cam.release(fb);
}
