#include "img/ESP32P4_Img.h"

#include <string.h>

void ESP32P4_Img::rgb565ToRgb888(const uint16_t *src, uint8_t *dst, size_t pixels) {
  for (size_t i = 0; i < pixels; i++) {
    uint16_t p = src[i];
    dst[i * 3 + 0] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
    dst[i * 3 + 1] = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
    dst[i * 3 + 2] = (uint8_t)((p & 0x1F) * 255 / 31);
  }
}

void ESP32P4_Img::rgb888ToRgb565(const uint8_t *src, uint16_t *dst, size_t pixels) {
  for (size_t i = 0; i < pixels; i++) {
    uint16_t r = src[i * 3 + 0] >> 3;
    uint16_t g = src[i * 3 + 1] >> 2;
    uint16_t b = src[i * 3 + 2] >> 3;
    dst[i] = (uint16_t)((r << 11) | (g << 5) | b);
  }
}

uint8_t ESP32P4_Img::luma565(uint16_t px) {
  uint32_t r = (px >> 11) & 0x1F;
  uint32_t g = (px >> 5) & 0x3F;
  uint32_t b = px & 0x1F;
  return (uint8_t)((r * 38 + g * 75 + b * 15) >> 5);
}

void ESP32P4_Img::histogram565(const uint16_t *src, size_t pixels, uint32_t bins[16]) {
  for (int i = 0; i < 16; i++) bins[i] = 0;
  for (size_t i = 0; i < pixels; i++) bins[luma565(src[i]) >> 4]++;
}

void ESP32P4_Img::crop565(const uint16_t *src, int sw, int sh, const esp32p4_rect_t &r, uint16_t *dst) {
  int x0 = r.x < 0 ? 0 : r.x;
  int y0 = r.y < 0 ? 0 : r.y;
  int x1 = r.x + r.w;
  int y1 = r.y + r.h;
  if (x1 > sw) x1 = sw;
  if (y1 > sh) y1 = sh;
  uint16_t *d = dst;
  for (int y = y0; y < y1; y++) {
    memcpy(d, src + y * sw + x0, (size_t)(x1 - x0) * 2);
    d += (x1 - x0);
  }
}

void ESP32P4_Img::downsample2x565(const uint16_t *src, int sw, int sh, uint16_t *dst) {
  int dw = sw / 2, dh = sh / 2;
  for (int y = 0; y < dh; y++) {
    const uint16_t *row = src + (y * 2) * sw;
    uint16_t *drow = dst + y * dw;
    for (int x = 0; x < dw; x++) drow[x] = row[x * 2];
  }
}

void ESP32P4_Img::fillRect565(uint16_t *img, int w, int h, const esp32p4_rect_t &r, uint16_t color,
                              int thickness) {
  int x0 = r.x, y0 = r.y, x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= w) x1 = w - 1;
  if (y1 >= h) y1 = h - 1;
  for (int t = 0; t < thickness; t++) {
    for (int x = x0; x <= x1; x++) {
      if (y0 + t < h) img[(y0 + t) * w + x] = color;
      if (y1 - t >= 0) img[(y1 - t) * w + x] = color;
    }
    for (int y = y0; y <= y1; y++) {
      if (x0 + t < w) img[y * w + x0 + t] = color;
      if (x1 - t >= 0) img[y * w + x1 - t] = color;
    }
  }
}

void ESP32P4_Img::blit565(const camera_fb_t *fb, uint16_t *dst, int dw, int dh) {
  if (!fb || !dst) return;
  int copy_w = fb->width < dw ? fb->width : dw;
  int copy_h = fb->height < dh ? fb->height : dh;
  const uint16_t *src = (const uint16_t *)fb->buf;
  for (int y = 0; y < copy_h; y++) {
    memcpy(dst + y * dw, src + y * fb->width, (size_t)copy_w * 2);
  }
}
