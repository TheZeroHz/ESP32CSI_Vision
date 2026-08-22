#pragma once

/**
 * ESP-DL hand detect + 8-gesture classification.
 * Weights: espdet_pico_224_224_hand.espdl + mobilenetv2_0_5_128_128_gesture.espdl
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "vision/ESP32P4_VisionAi.h"

class ESP32P4_Gesture {
 public:
  ESP32P4_Gesture() = default;
  ~ESP32P4_Gesture() { end(); }

  bool begin();
  void end();
  bool ready() const { return _hand != nullptr && _cls != nullptr; }

  void setScoreThr(float thr);
  float scoreThr() const { return _score_thr; }

  int detect(const uint16_t *rgb565, int w, int h, esp32p4_gesture_t *out, int max_out);
  int detect(const camera_fb_t *fb, esp32p4_gesture_t *out, int max_out);

  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }

  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_gesture_t *g, int n,
                   uint16_t color = 0xFD20);

 private:
  bool ensureRgb(size_t pixels);

  void *_hand = nullptr;
  void *_cls = nullptr;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  float _score_thr = 0.25f;
  int _last_ms = 0;
  int _last_n = 0;
};
