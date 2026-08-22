#pragma once

/**
 * ESP-DL ImageNet classification (MobileNetV2, 1000 classes).
 * Weights: models/espdl/p4/imagenet_cls_mobilenetv2_s8_v1.espdl
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "vision/ESP32P4_VisionAi.h"

class ESP32P4_Cls {
 public:
  ESP32P4_Cls() = default;
  ~ESP32P4_Cls() { end(); }

  bool begin(int topk = 5);
  void end();
  bool ready() const { return _impl != nullptr; }

  void setTopk(int k);
  int topk() const { return _topk; }

  int classify(const uint16_t *rgb565, int w, int h, esp32p4_cls_t *out, int max_out);
  int classify(const camera_fb_t *fb, esp32p4_cls_t *out, int max_out);

  int lastMs() const { return _last_ms; }

 private:
  bool ensureRgb(size_t pixels);

  void *_impl = nullptr;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  int _topk = 5;
  int _last_ms = 0;
};
