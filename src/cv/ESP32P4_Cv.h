#pragma once

/**
 * Lightweight OpenCV-inspired imgproc for ESP32-P4 CSI frames (RGB565 / GRAY8).
 * Patterns from micropython-opencv / esp32-opencv (lean imgproc, PSRAM buffers,
 * dst reuse) — accelerated with ESP32-P4 PPA (RGB→GRAY, scale) where available.
 * No full OpenCV dependency.
 */

#include <stddef.h>
#include <stdint.h>

#include "img/ESP32P4_Img.h"

struct esp32p4_hsv_t {
  uint8_t h;  // 0..179 (OpenCV-style half-degree)
  uint8_t s;  // 0..255
  uint8_t v;  // 0..255
};

struct esp32p4_blob_t {
  esp32p4_rect_t box;
  int area;
  int cx;
  int cy;
};

enum esp32p4_thresh_t : uint8_t {
  ESP32P4_THRESH_BINARY = 0,
  ESP32P4_THRESH_BINARY_INV = 1,
};

class ESP32P4_Cv {
 public:
  /**
   * RGB565 → GRAY8. Uses PPA SRM when roi==null (HW); else fast software.
   * For half-res CV prefer toGrayScale() (one PPA pass).
   */
  static bool toGray(const uint16_t *src, int w, int h, uint8_t *dst,
                     const esp32p4_rect_t *roi = nullptr);

  /** RGB565 → GRAY8 at dw×dh via PPA (or SW downsample). OpenCV-style pyramid step. */
  static bool toGrayScale(const uint16_t *src, int sw, int sh, uint8_t *dst, int dw, int dh);

  /** GRAY8 2× downsample (nearest). */
  static bool downsample2x(const uint8_t *src, int sw, int sh, uint8_t *dst);

  /** 3×3 box blur (separable 3+3) on GRAY8. */
  static bool blur3x3(const uint8_t *src, int w, int h, uint8_t *dst,
                      const esp32p4_rect_t *roi = nullptr);

  /** Binary threshold on GRAY8. */
  static bool threshold(const uint8_t *src, int w, int h, uint8_t *dst, uint8_t thr,
                        esp32p4_thresh_t type = ESP32P4_THRESH_BINARY,
                        const esp32p4_rect_t *roi = nullptr);

  /** Otsu threshold (0..255) from GRAY8 histogram — classic OpenCV. */
  static uint8_t otsu(const uint8_t *gray, size_t pixels);

  /**
   * Adaptive mean threshold (OpenCV ADAPTIVE_THRESH_MEAN_C).
   * block_size must be odd >= 3. dark=true → BINARY_INV (pixel < localMean-C).
   * integral_scratch: (w+1)*(h+1) uint32_t in PSRAM, or null to allocate temporarily.
   */
  static bool adaptiveThreshold(const uint8_t *src, int w, int h, uint8_t *dst, int block_size,
                                int C, bool dark, uint32_t *integral_scratch = nullptr);

  /** Morphology open = erode then dilate; close = dilate then erode. */
  static bool morphologyOpen(const uint8_t *src, int w, int h, uint8_t *dst, int iterations = 1);
  static bool morphologyClose(const uint8_t *src, int w, int h, uint8_t *dst, int iterations = 1);

  /** Convert one RGB565 pixel → HSV (H 0..179). */
  static esp32p4_hsv_t rgb565ToHsv(uint16_t px);

  /**
   * Color band mask (OpenCV inRange style) on RGB565 → GRAY8 binary (0/255).
   * H wraps if lo.h > hi.h (red straddling 0).
   */
  static bool inRangeHsv(const uint16_t *src, int w, int h, uint8_t *dst, esp32p4_hsv_t lo,
                         esp32p4_hsv_t hi, const esp32p4_rect_t *roi = nullptr);

  /** 3×3 erode / dilate on binary GRAY8 (0 or non-zero). */
  static bool erode(const uint8_t *src, int w, int h, uint8_t *dst, int iterations = 1,
                    const esp32p4_rect_t *roi = nullptr);
  static bool dilate(const uint8_t *src, int w, int h, uint8_t *dst, int iterations = 1,
                     const esp32p4_rect_t *roi = nullptr);

  /**
   * Edge map: Sobel magnitude + dual threshold (Canny-inspired, no full hysteresis).
   * src/dst GRAY8; scratch must be nullptr or w*h*2 bytes (gx/gy packed) — if null, uses stack
   * only for tiny ROIs; for full frames pass PSRAM scratch of w*h uint16_t (mag).
   */
  static bool edges(const uint8_t *src, int w, int h, uint8_t *dst, uint8_t thr_lo = 40,
                    uint8_t thr_hi = 100, uint16_t *mag_scratch = nullptr,
                    const esp32p4_rect_t *roi = nullptr);

  /**
   * Connected-component blobs on binary GRAY8 (ESP-VISION find_blobs style).
   * Returns count written to out (capped by max_out). min_area filters noise.
   * label_scratch: w*h uint16_t in PSRAM (required for large frames).
   */
  static int findBlobs(const uint8_t *bin, int w, int h, esp32p4_blob_t *out, int max_out,
                       int min_area, uint16_t *label_scratch,
                       const esp32p4_rect_t *roi = nullptr);

  /** Annotate RGB565 (esp-vision / OpenCV draw_* style). */
  static void line(uint16_t *img, int w, int h, int x0, int y0, int x1, int y1, uint16_t color,
                   int thickness = 1);
  static void circle(uint16_t *img, int w, int h, int cx, int cy, int radius, uint16_t color,
                     int thickness = 1);
  static void putText(uint16_t *img, int w, int h, int x, int y, const char *text, uint16_t color,
                      int scale = 1);

  /** Cross-mark blob centers / boxes onto RGB565. */
  static void drawBlob(uint16_t *img, int w, int h, const esp32p4_blob_t &b, uint16_t color,
                       int thickness = 2);
};
