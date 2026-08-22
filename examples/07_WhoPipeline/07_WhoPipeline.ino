#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "07_WhoPipeline"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif


ESP32P4_Camera cam;
ESP32P4_WhoPipeline who;

static void on_frame(const esp32p4_who_fb_t *fb, void *) {
  Serial.printf("who cb %ux%u ts=%u\n", fb->width, fb->height, (unsigned)fb->timestamp_us);
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 07_WhoPipeline ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  if (!cam.begin(esp32csi_cam_config())) {
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
  dbg.poll();
  esp32p4_who_fb_t fb{};
  if (who.waitFrame(&fb, 2000)) {
    Serial.printf("who wait %ux%u len=%u\n", fb.width, fb.height, (unsigned)fb.len);
  }
}
