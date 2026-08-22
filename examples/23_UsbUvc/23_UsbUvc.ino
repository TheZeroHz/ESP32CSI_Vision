#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "23_UsbUvc"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM | ESP32P4_DBG_JPEG
#endif


// USB device UVC gadget: CSI → HW JPEG → PC webcam.
// Not V4L2. Serial stays on UART (115200). Native USB-C is the camera.
//
// Arduino IDE:
//   USB Mode          = USB-OTG (TinyUSB)
//   USB CDC On Boot   = Disabled   (so advertised size matches CSI)
//
// Then plug the P4 USB-C into the PC and open the Camera / Cheese / OBS.

ESP32P4_Camera cam;
ESP32P4_Uvc uvc;  // global: registers UVC before TinyUSB starts

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 23_UsbUvc ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera begin FAILED");
    while (true) delay(1000);
  }
  Serial.printf("CSI: %s  %ux%u\n", cam.sensorName(), cam.width(), cam.height());
  if (!uvc.begin(&cam, 45)) {
    Serial.println("UVC begin FAILED - USB-OTG TinyUSB required");
    while (true) delay(1000);
  }
}

void loop() {
  dbg.poll();
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 2000) return;
  last = now;
  Serial.printf("UVC frames=%u streaming=%u psram_free=%u\n", (unsigned)uvc.frames(),
                (unsigned)uvc.streaming(), (unsigned)esp32p4_psram_free_size());
}
