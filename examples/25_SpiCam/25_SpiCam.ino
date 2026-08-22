/**
 * 25_SpiCam — SPI camera (SP0A39 1-bit gray VGA). Not MIPI CSI.
 *
 * Pins / sensor / size: board_config.h in this folder (CFG_SPI_*).
 */

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "25_SpiCam"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif

ESP32P4_Camera cam;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 25_SpiCam ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  if (!cam.begin(cfg)) {
    Serial.println("SPI begin FAILED — CFG_SPI_* in board_config.h, or Arduino lib lacks SPI cam");
    while (true) delay(1000);
  }
  Serial.printf("%s %s  %ux%u  %s\n", cam.busName(), cam.sensorName(), cam.width(), cam.height(),
                cam.formatName());
}

void loop() {
  dbg.poll();
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) {
    Serial.println("capture timeout");
    return;
  }
  Serial.printf("fb %ux%u  %u bytes  GRAY8  done=%u drop=%u\n", fb->width, fb->height,
                (unsigned)fb->len, (unsigned)cam.doneCount(), (unsigned)cam.dropCount());
  cam.release(fb);
  delay(500);
}
