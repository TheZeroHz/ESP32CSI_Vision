#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;
ESP32P4_WhoPipeline who;

static void on_frame(const esp32p4_who_fb_t *fb, void *) {
  Serial.printf("who cb %ux%u ts=%u\n", fb->width, fb->height, (unsigned)fb->timestamp_us);
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 07_WhoPipeline ===");
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }
  who.onFrame(on_frame);
  if (!who.begin(&cam, 2)) {
    Serial.println("pipeline FAILED");
    while (true) delay(1000);
  }
}

void loop() {
  esp32p4_who_fb_t fb{};
  if (who.waitFrame(&fb, 2000)) {
    Serial.printf("who wait %ux%u len=%u\n", fb.width, fb.height, (unsigned)fb.len);
  }
}
