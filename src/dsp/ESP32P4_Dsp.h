#pragma once

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "img/ESP32P4_Img.h"

struct esp32p4_motion_t {
  bool moving;
  uint32_t changed;
  uint32_t total;
  esp32p4_rect_t roi;
};

class ESP32P4_Dsp {
 public:
  bool begin(uint16_t w, uint16_t h, uint8_t threshold = 25);
  void end();
  bool detect(const camera_fb_t *fb, esp32p4_motion_t *out);

 private:
  uint8_t *_prev = nullptr;
  uint16_t _dw = 0;
  uint16_t _dh = 0;
  uint8_t _thr = 25;
  bool _has_prev = false;
};
