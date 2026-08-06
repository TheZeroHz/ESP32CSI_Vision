#include "dsp/ESP32P4_Dsp.h"

#include "mem/ESP32P4_Psram.h"

bool ESP32P4_Dsp::begin(uint16_t w, uint16_t h, uint8_t threshold) {
  end();
  _dw = w / 8;
  _dh = h / 8;
  if (_dw < 8) _dw = 8;
  if (_dh < 8) _dh = 8;
  _thr = threshold;
  _prev = (uint8_t *)esp32p4_psram_alloc((size_t)_dw * _dh);
  _has_prev = false;
  return _prev != nullptr;
}

void ESP32P4_Dsp::end() {
  esp32p4_psram_free(_prev);
  _prev = nullptr;
  _has_prev = false;
}

bool ESP32P4_Dsp::detect(const camera_fb_t *fb, esp32p4_motion_t *out) {
  if (!fb || !_prev || !out) return false;
  const uint16_t *p = (const uint16_t *)fb->buf;
  int minx = _dw, miny = _dh, maxx = 0, maxy = 0;
  uint32_t changed = 0;
  uint32_t total = (uint32_t)_dw * _dh;
  for (int y = 0; y < _dh; y++) {
    for (int x = 0; x < _dw; x++) {
      int sx = x * fb->width / _dw;
      int sy = y * fb->height / _dh;
      uint8_t lum = ESP32P4_Img::luma565(p[sy * fb->width + sx]);
      size_t i = (size_t)y * _dw + x;
      if (_has_prev) {
        int d = (int)lum - (int)_prev[i];
        if (d < 0) d = -d;
        if (d > _thr) {
          changed++;
          if (x < minx) minx = x;
          if (y < miny) miny = y;
          if (x > maxx) maxx = x;
          if (y > maxy) maxy = y;
        }
      }
      _prev[i] = lum;
    }
  }
  _has_prev = true;
  out->changed = changed;
  out->total = total;
  out->moving = _has_prev && changed > (total / 80);
  if (out->moving) {
    out->roi.x = minx * fb->width / _dw;
    out->roi.y = miny * fb->height / _dh;
    out->roi.w = (maxx - minx + 1) * fb->width / _dw;
    out->roi.h = (maxy - miny + 1) * fb->height / _dh;
  } else {
    out->roi = {0, 0, 0, 0};
  }
  return true;
}
