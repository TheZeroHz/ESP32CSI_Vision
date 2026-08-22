#pragma once

/**
 * ESP-DL PaddleOCR v6 (detect + recognize).
 * Weights: pp_ocr_v6_det_s8.espdl + pp_ocr_v6_rec_*.espdl
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "vision/ESP32P4_VisionAi.h"

enum esp32p4_ocr_rec_model_t : int {
  ESP32P4_OCR_REC_S8 = 0,
  ESP32P4_OCR_REC_S16 = 1,
};
enum esp32p4_ocr_rec_mode_t : int {
  ESP32P4_OCR_SHORT = 0,  // 48x320
  ESP32P4_OCR_DUAL = 1,   // 48x320 + 48x640
};

class ESP32P4_Ocr {
 public:
  enum RecModel : int {
    REC_S8 = ESP32P4_OCR_REC_S8,
    REC_S16 = ESP32P4_OCR_REC_S16,
  };
  enum RecMode : int {
    SHORT = ESP32P4_OCR_SHORT,
    DUAL = ESP32P4_OCR_DUAL,
  };

  ESP32P4_Ocr() = default;
  ~ESP32P4_Ocr() { end(); }

  bool begin(RecModel rec = REC_S16, RecMode mode = SHORT);
  void end();
  bool ready() const { return _impl != nullptr; }

  void setScoreThr(float thr);
  float scoreThr() const { return _score_thr; }

  int run(const uint16_t *rgb565, int w, int h, esp32p4_ocr_t *out, int max_out);
  int run(const camera_fb_t *fb, esp32p4_ocr_t *out, int max_out);

  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }

  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_ocr_t *hits, int n,
                   uint16_t color = 0xFFE0);

 private:
  bool ensureRgb(size_t pixels);

  void *_impl = nullptr;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  float _score_thr = 0.5f;
  int _last_ms = 0;
  int _last_n = 0;
};
