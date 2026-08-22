#pragma once

/**
 * ESP-DL COCO instance segmentation (YOLO11n-Seg).
 * Weights: models/espdl/p4/coco_seg_yolo11n_seg_s8_v1.espdl
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "vision/ESP32P4_VisionAi.h"

class ESP32P4_Seg {
 public:
  enum Model : int { YOLO11N_SEG_V1 = 0 };

  ESP32P4_Seg() = default;
  ~ESP32P4_Seg() { end(); }

  bool begin(Model model = YOLO11N_SEG_V1);
  void end();
  bool ready() const { return _impl != nullptr; }

  void setScoreThr(float thr);
  float scoreThr() const { return _score_thr; }

  /** Mask pointers in `out` stay valid until the next detect() / end(). */
  int detect(const uint16_t *rgb565, int w, int h, esp32p4_seg_t *out, int max_out);
  int detect(const camera_fb_t *fb, esp32p4_seg_t *out, int max_out);

  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }

  static const char *label(int category);
  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_seg_t *dets, int n);

 private:
  bool ensureRgb(size_t pixels);

  void *_impl = nullptr;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  uint8_t *_mask_arena = nullptr;
  size_t _mask_arena_cap = 0;
  float _score_thr = 0.25f;
  int _last_ms = 0;
  int _last_n = 0;
};
