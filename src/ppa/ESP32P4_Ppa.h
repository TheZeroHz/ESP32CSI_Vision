#pragma once

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"

class ESP32P4_Ppa {
 public:
  bool begin();
  void end();
  bool scale(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w, uint16_t dst_h);
  bool rotate90(const camera_fb_t *src, uint8_t *dst, size_t dst_cap);
  bool mirror(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, bool mx, bool my);

 private:
  bool srm(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w, uint16_t dst_h,
           int rot90s, bool mx, bool my);
  void *_client = nullptr;
};
