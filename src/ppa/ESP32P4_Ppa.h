#pragma once

/**
 * ESP32-P4 Pixel-Processing Accelerator (PPA) helpers.
 * SRM: scale / rotate / mirror / RGB565↔GRAY8. FILL: solid rects.
 */

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

  /** Scale RGB565 buffer → RGB565 (PPA SRM). */
  bool scaleRgb565(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh);

  /**
   * Uniform letterbox fit into destination (cleared bars).
   * Destination may be any even size (face model inputs need not be ÷16).
   */
  bool scaleFit(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                uint16_t dst_h);

  /** Uniform cover: center-crop source to dst aspect, scale to fill (no bars). */
  bool scaleCover(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                  uint16_t dst_h);

  /** RGB565 → GRAY8 same size (PPA SRM color convert). Falls back false on HW fail. */
  bool rgb565ToGray(const uint16_t *src, int w, int h, uint8_t *dst);

  /** RGB565 → GRAY8 with scale (one PPA pass). Ideal for half-res CV. */
  bool rgb565ToGrayScale(const uint16_t *src, int sw, int sh, uint8_t *dst, int dw, int dh);

  /** Hardware solid fill of an RGB565 rectangle (inclusive size rw×rh). */
  bool fillRect565(uint16_t *img, int w, int h, int x, int y, int rw, int rh, uint16_t color);

  /** Shared singleton for CV (separate from stream scaling client). */
  static ESP32P4_Ppa &cv();

 private:
  bool ensureSrm();
  bool ensureFill();
  bool srm(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w, uint16_t dst_h,
           int rot90s, bool mx, bool my);
  bool srmRaw(const void *src, int sw, int sh, int src_cm, void *dst, size_t dst_cap, int dw, int dh,
              int dst_cm, float sx, float sy);

  void *_srm = nullptr;
  void *_fill = nullptr;
};
