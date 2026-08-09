#pragma once

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"

class ESP32P4_Jpeg {
 public:
  bool begin(uint16_t max_w = 800, uint16_t max_h = 640, uint8_t quality = 45);
  void end();
  size_t encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap);
  size_t encode(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out, size_t out_cap);
  bool decodeInfo(const uint8_t *jpg, size_t jpg_len, uint32_t *w, uint32_t *h);
  size_t decode(const uint8_t *jpg, size_t jpg_len, uint8_t *rgb_out, size_t out_cap, uint32_t *w,
                uint32_t *h);
  void setQuality(uint8_t q) { _quality = q < 1 ? 1 : (q > 100 ? 100 : q); }
  /** Wipe encoder input so a smaller size cannot leak pixels from a prior frame. */
  void clearInput();

 private:
  void *_enc = nullptr;
  void *_dec = nullptr;
  uint8_t *_in = nullptr;
  size_t _in_cap = 0;
  uint8_t _quality = 45;
  uint16_t _max_w = 0;
  uint16_t _max_h = 0;
  uint16_t _last_w = 0;
  uint16_t _last_h = 0;
};
