#pragma once

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"

struct esp32p4_rect_t {
  int x, y, w, h;
};

class ESP32P4_Img {
 public:
  static void rgb565ToRgb888(const uint16_t *src, uint8_t *dst, size_t pixels);
  static void rgb888ToRgb565(const uint8_t *src, uint16_t *dst, size_t pixels);
  static uint8_t luma565(uint16_t px);
  static void histogram565(const uint16_t *src, size_t pixels, uint32_t bins[16]);
  static void crop565(const uint16_t *src, int sw, int sh, const esp32p4_rect_t &r, uint16_t *dst);
  static void downsample2x565(const uint16_t *src, int sw, int sh, uint16_t *dst);
  static void fillRect565(uint16_t *img, int w, int h, const esp32p4_rect_t &r, uint16_t color,
                          int thickness = 2);
  static void blit565(const camera_fb_t *fb, uint16_t *dst, int dw, int dh);
};
