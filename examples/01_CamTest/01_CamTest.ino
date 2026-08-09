#include <ESP32CSI_Vision.h>

// Board preset (pins + LDO). Sensor defaults to ESP32P4_SENSOR_AUTO probe.
//   GUITION_M3 / WAVESHARE_NANO → OV5647 / IMX708 common
//   FUNCTION_EV                 → SC2336 typical
#ifndef APP_BOARD
#define APP_BOARD ESP32P4_BOARD_GUITION_M3
#endif

ESP32P4_Camera cam;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 01_CamTest ===");
  Serial.println("CSI: begin AUTO probe — see Serial for registry hits");
  if (!cam.begin(APP_BOARD)) {
    Serial.println("camera begin FAILED");
    while (true) delay(1000);
  }
  Serial.printf("CSI: %s @ 0x%02X  %ux%u  lanes=%u\n", cam.sensorName(), cam.sensorAddress(),
                cam.width(), cam.height(), (unsigned)cam.dataLanes());
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
