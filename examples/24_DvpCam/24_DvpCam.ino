/**
 * 24_DvpCam — DVP parallel camera (OV2640 VGA RGB565). Not MIPI CSI.
 *
 * Pins / sensor / size: board_config.h in this folder (Arduino IDE tab).
 */

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "24_DvpCam"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif

ESP32P4_Camera cam;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 24_DvpCam ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  if (!cam.begin(cfg)) {
    Serial.println("DVP begin FAILED — CFG_DVP_* / OV2640 SCCB in board_config.h");
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
  Serial.printf("fb %ux%u  %u bytes  %s  done=%u drop=%u\n", fb->width, fb->height,
                (unsigned)fb->len, cam.formatName(), (unsigned)cam.doneCount(),
                (unsigned)cam.dropCount());
  cam.release(fb);
  delay(500);
}
