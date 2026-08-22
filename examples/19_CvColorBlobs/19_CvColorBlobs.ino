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

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "19_CvColorBlobs"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif


ESP32P4_Camera cam;

// Tune for your target color (default: saturated reds, H wraps 0)
static esp32p4_hsv_t LO{170, 80, 60};
static esp32p4_hsv_t HI{10, 255, 255};

static uint8_t *mask = nullptr;
static uint8_t *tmp = nullptr;
static uint16_t *labels = nullptr;
static uint8_t *tensor = nullptr;  // letterbox RGB888

ESP32P4_Cv cv;
ESP32P4_VisionAi vai;
ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 19_CvColorBlobs (cv + vision AI helpers) ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  if (!cam.begin(esp32csi_cam_config())) {
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
  dbg.poll();
  camera_fb_t *fb = cam.capture();
  if (!fb) return;

  const int w = fb->width;
  const int h = fb->height;
  auto *rgb = (uint16_t *)fb->buf;

  cv.inRangeHsv(rgb, w, h, mask, LO, HI);
  cv.erode(mask, w, h, tmp, 1);
  cv.dilate(tmp, w, h, mask, 2);

  esp32p4_blob_t blobs[8];
  int n = cv.findBlobs(mask, w, h, blobs, 8, 400, labels);

  const uint16_t green = 0x07E0;
  for (int i = 0; i < n; i++) {
    cv.drawBlob(rgb, w, h, blobs[i], green, 2);
    Serial.printf("blob[%d] area=%d cx=%d cy=%d box=%d,%d %dx%d\n", i, blobs[i].area, blobs[i].cx,
                  blobs[i].cy, blobs[i].box.x, blobs[i].box.y, blobs[i].box.w, blobs[i].box.h);
  }

  esp32p4_letterbox_t lb{};
  if (vai.letterboxRgb565(rgb, w, h, tensor, 224, 224, &lb)) {
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
