#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;
ESP32P4_Ppa ppa;
uint8_t *scaled = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 06_PpaScale ===");
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }
  if (!ppa.begin()) {
    Serial.println("PPA FAILED — falling back to CPU downsample in loop");
  }
  scaled = (uint8_t *)esp32p4_psram_alloc(400 * 320 * 2);
}

void loop() {
  camera_fb_t *fb = cam.capture();
  if (!fb || !scaled) return;
  bool ok = ppa.scale(fb, scaled, 400 * 320 * 2, 400, 320);
  if (!ok) {
    ESP32P4_Img::downsample2x565((const uint16_t *)fb->buf, fb->width, fb->height,
                                 (uint16_t *)scaled);
    Serial.println("cpu 400x320 downsample");
  } else {
    Serial.println("ppa 400x320 ok");
  }
  cam.release(fb);
  delay(200);
}
