#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;
ESP32P4_Jpeg jpeg;
uint8_t *jpg = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 02_JpegSnapshot ===");
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }
  if (!jpeg.begin(cam.width(), cam.height(), 45)) {
    Serial.println("jpeg FAILED");
    while (true) delay(1000);
  }
  jpg = (uint8_t *)esp32p4_psram_alloc(200 * 1024);
}

void loop() {
  camera_fb_t *fb = cam.capture();
  if (!fb || !jpg) return;
  size_t n = jpeg.encode(fb, jpg, 200 * 1024);
  cam.release(fb);
  Serial.printf("jpeg %u bytes  soi=%02X%02X eoi=%02X%02X\n", (unsigned)n,
                n > 1 ? jpg[0] : 0, n > 1 ? jpg[1] : 0, n > 1 ? jpg[n - 2] : 0,
                n > 1 ? jpg[n - 1] : 0);
  delay(500);
}
