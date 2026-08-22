/**
 * 26_UsbHostUvc — USB host UVC webcam on the P4 OTG port.
 *
 * capture() returns JPEG (MJPEG) or YUY2 as YUV422. Not V4L2. Not the USB
 * device gadget (example 23_UsbUvc). VID/PID/size: board_config.h CFG_UVC_*.
 *
 * Do not use ESP32P4_Uvc in this sketch — gadget and host cannot share the PHY.
 * Arduino IDE: leave the OTG port as host, CDC on UART.
 */

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "26_UsbHostUvc"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif

ESP32P4_Camera cam;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 26_UsbHostUvc ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  if (!cam.begin(cfg)) {
    Serial.println("UVC host begin FAILED — plug a webcam, check USB host mode / CFG_UVC_*");
    while (true) delay(1000);
  }
  Serial.printf("%s  %ux%u  %s\n", cam.busName(), cam.width(), cam.height(), cam.formatName());
}

void loop() {
  dbg.poll();
  camera_fb_t *fb = cam.capture(3000);
  if (!fb) {
    Serial.println("capture timeout");
    return;
  }
  Serial.printf("fb %ux%u  %u bytes  %s  done=%u drop=%u\n", fb->width, fb->height,
                (unsigned)fb->len, esp32p4_pixformat_name(fb->format), (unsigned)cam.doneCount(),
                (unsigned)cam.dropCount());
  cam.release(fb);
  delay(200);
}
