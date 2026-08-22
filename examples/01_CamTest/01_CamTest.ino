/**
 * 01_CamTest — CSI capture. Any vendor ESP32-P4.
 *
 * Set camera GPIOs in board_config.h
 * (docs/Custom-Boards.md). This sketch does not assume Guition.
 */

#ifndef APP_NAME
#define APP_NAME "01_CamTest"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif

#include "board_config.h"

ESP32P4_Camera cam;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 01_CamTest ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  /* Still yours after the helper, e.g. cfg.sensor = ESP32P4_SENSOR_IMX477; */
  esp32csi_print_cam_config(cfg);

  if (!cam.begin(cfg)) {
    Serial.println("camera begin FAILED — check ESP32CSI_CAM_* vs schematic");
    while (true) delay(1000);
  }
  Serial.printf("CSI: %s @ 0x%02X  %ux%u  lanes=%u  %s\n", cam.sensorName(), cam.sensorAddress(),
                cam.width(), cam.height(), (unsigned)cam.dataLanes(), cam.formatName());
}

void loop() {
  dbg.poll();
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) {
    Serial.println("capture timeout");
    return;
  }
  uint32_t sum = 0;
  size_t n = fb->len / 2;
  const uint16_t *p = (const uint16_t *)fb->buf;
  for (size_t i = 0; i < n; i += 16) sum += p[i];
  Serial.printf("fb %ux%u  %u bytes  %s  checksum=%u  done=%u\n", fb->width, fb->height,
                (unsigned)fb->len, cam.formatName(), (unsigned)sum, (unsigned)cam.doneCount());
  cam.release(fb);
}
