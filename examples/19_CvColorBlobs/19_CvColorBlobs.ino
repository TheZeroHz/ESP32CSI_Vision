/**
 * 19_CvColorBlobs — OpenCV-inspired color blobs + VisionAi letterbox demo
 *
 * Inspired by micropython-opencv (inRange / morph / annotate) and
 * ESP-VISION (find_blobs + letterbox preprocess for ESP-DL).
 *
 * Board: Guition JC-ESP32P4-M3
 * Serial @ 115200 · PSRAM on
 *
 * Pipeline each frame:
 *   RGB565 → HSV inRange → erode/dilate → findBlobs → draw on FB
 *   Also builds a 224×224 letterbox RGB888 tensor (ESP-DL input shape)
 */

#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;

// Tune for your target color (default: saturated reds, H wraps 0)
static esp32p4_hsv_t LO{170, 80, 60};
static esp32p4_hsv_t HI{10, 255, 255};

static uint8_t *mask = nullptr;
static uint8_t *tmp = nullptr;
static uint16_t *labels = nullptr;
static uint8_t *tensor = nullptr;  // letterbox RGB888

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 19_CvColorBlobs (cv + vision AI helpers) ===");

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  const int w = cam.width();
  const int h = cam.height();
  const size_t px = (size_t)w * (size_t)h;

  mask = (uint8_t *)esp32p4_psram_alloc(px);
  tmp = (uint8_t *)esp32p4_psram_alloc(px);
  labels = (uint16_t *)esp32p4_psram_alloc(px * sizeof(uint16_t));
  tensor = (uint8_t *)esp32p4_psram_alloc(224 * 224 * 3);
  if (!mask || !tmp || !labels || !tensor) {
    Serial.println("PSRAM buffers FAILED");
    while (true) delay(1000);
  }

  Serial.printf("frame %dx%d  HSV lo=%u,%u,%u hi=%u,%u,%u\n", w, h, LO.h, LO.s, LO.v, HI.h, HI.s,
                HI.v);
  Serial.println("Point a saturated red object at the camera");
}

void loop() {
  camera_fb_t *fb = cam.capture();
  if (!fb) return;

  const int w = fb->width;
  const int h = fb->height;
  auto *rgb = (uint16_t *)fb->buf;

  ESP32P4_Cv::inRangeHsv(rgb, w, h, mask, LO, HI);
  ESP32P4_Cv::erode(mask, w, h, tmp, 1);
  ESP32P4_Cv::dilate(tmp, w, h, mask, 2);

  esp32p4_blob_t blobs[8];
  int n = ESP32P4_Cv::findBlobs(mask, w, h, blobs, 8, 400, labels);

  const uint16_t green = 0x07E0;
  for (int i = 0; i < n; i++) {
    ESP32P4_Cv::drawBlob(rgb, w, h, blobs[i], green, 2);
    Serial.printf("blob[%d] area=%d cx=%d cy=%d box=%d,%d %dx%d\n", i, blobs[i].area, blobs[i].cx,
                  blobs[i].cy, blobs[i].box.x, blobs[i].box.y, blobs[i].box.w, blobs[i].box.h);
  }

  esp32p4_letterbox_t lb{};
  if (ESP32P4_VisionAi::letterboxRgb565(rgb, w, h, tensor, 224, 224, &lb)) {
    static uint32_t last = 0;
    if (millis() - last > 2000) {
      last = millis();
      Serial.printf("letterbox scale=%.3f pad=%d,%d (ready for ESP-DL 224 input)\n", lb.scale,
                    lb.pad_x, lb.pad_y);
    }
  }

  if (n == 0) {
    static uint32_t idle = 0;
    if (millis() - idle > 1500) {
      idle = millis();
      Serial.println("(no blobs)");
    }
  }

  cam.release(fb);
}
