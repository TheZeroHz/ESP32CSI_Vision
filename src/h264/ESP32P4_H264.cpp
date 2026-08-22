#include "h264/ESP32P4_H264.h"

#include <Arduino.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// HW encoder: espressif/esp_h264 is vendored under src/ (headers) + src/esp_h264/
// for Arduino-ESP32. ESP-IDF builds use the Component Manager dependency instead.
#if CONFIG_IDF_TARGET_ESP32P4
#define ESP32P4_HAS_ESP_H264 1
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_h264_enc_param.h"
#include "esp_h264_enc_param_hw.h"
#include "esp_h264_enc_hw_param.h"
#include "esp_h264_enc_single_hw.h"
#include "h264/ESP32P4_H264Mp4.h"
#include "mem/ESP32P4_Psram.h"
#else
#define ESP32P4_HAS_ESP_H264 0
#endif

#if ESP32P4_HAS_ESP_H264

static inline uint8_t rgb565_r(uint16_t p) { return (uint8_t)(((p >> 11) & 0x1F) * 255 / 31); }
static inline uint8_t rgb565_g(uint16_t p) { return (uint8_t)(((p >> 5) & 0x3F) * 255 / 63); }
static inline uint8_t rgb565_b(uint16_t p) { return (uint8_t)((p & 0x1F) * 255 / 31); }

static inline void rgb_to_yuv(uint8_t r, uint8_t g, uint8_t b, uint8_t *y, uint8_t *u, uint8_t *v) {
  int yy = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
  int uu = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
  int vv = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
  *y = (uint8_t)(yy < 0 ? 0 : (yy > 255 ? 255 : yy));
  *u = (uint8_t)(uu < 0 ? 0 : (uu > 255 ? 255 : uu));
  *v = (uint8_t)(vv < 0 ? 0 : (vv > 255 ? 255 : vv));
}

static void *h264_aligned_alloc(size_t bytes, uint32_t *actual) {
  const size_t align = 128;
  size_t need = (bytes + align - 1) & ~(align - 1);
  void *p = heap_caps_aligned_calloc(align, 1, need, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_aligned_calloc(align, 1, need, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!p) p = esp32p4_psram_alloc(need, align);
  if (actual) *actual = p ? (uint32_t)need : 0;
  return p;
}

esp32p4_h264_cfg_t esp32p4_h264_cfg_default(uint16_t w, uint16_t h) {
  esp32p4_h264_cfg_t c{};
  c.width = (uint16_t)((w + 15) & ~15u);
  c.height = (uint16_t)((h + 15) & ~15u);
  if (c.width < 80) c.width = 80;
  if (c.height < 80) c.height = 80;
  c.fps = 15;
  c.gop = 15;
  c.bitrate = (uint32_t)c.width * c.height * c.fps / 8;
  if (c.bitrate < 400000) c.bitrate = 400000;
  c.qp_min = 24;
  c.qp_max = 36;
  c.input_format = ESP32P4_PIXFORMAT_RGB565;
  return c;
}

bool ESP32P4_H264::begin(uint16_t w, uint16_t h, uint8_t fps, uint32_t bitrate) {
  esp32p4_h264_cfg_t cfg = esp32p4_h264_cfg_default(w, h);
  if (fps) {
    cfg.fps = fps;
    cfg.gop = fps;
  }
  if (bitrate) cfg.bitrate = bitrate;
  return begin(cfg);
}

ESP32P4_H264::PicKind ESP32P4_H264::picKindFor(esp32p4_cam_pixformat_t fmt) const {
  switch (fmt) {
    case ESP32P4_PIXFORMAT_YUV420:
    case ESP32P4_PIXFORMAT_GRAY8:
      return PIC_YUV_PACKED;
    case ESP32P4_PIXFORMAT_YUV422:
      return PIC_UYVY;
    case ESP32P4_PIXFORMAT_YUYV:
      return PIC_YUYV;
    default:
      return PIC_RGB565;
  }
}

const char *ESP32P4_H264::inputName() const {
  switch (_pic) {
    case PIC_YUV_PACKED:
      return "YUV420-packed";
    case PIC_UYVY:
      return "UYVY";
    case PIC_YUYV:
      return "YUYV";
    case PIC_RGB565:
      return "RGB565";
    default:
      return "none";
  }
}

void ESP32P4_H264::dropEncoder() {
  if (_enc) {
    auto enc = (esp_h264_enc_handle_t)_enc;
    if (_opened) esp_h264_enc_close(enc);
    esp_h264_enc_del(enc);
    _enc = nullptr;
  }
  _param = nullptr;
  _opened = false;
  _use_rgb565 = false;
  _pic = PIC_NONE;
}

bool ESP32P4_H264::createEncoder(PicKind kind) {
  if (kind == PIC_NONE) return false;
  esp_h264_enc_cfg_hw_t hcfg = {};
  hcfg.gop = _cfg.gop;
  hcfg.fps = _cfg.fps;
  hcfg.res.width = _cfg.width;
  hcfg.res.height = _cfg.height;
  hcfg.rc.bitrate = _cfg.bitrate;
  hcfg.rc.qp_min = _cfg.qp_min;
  hcfg.rc.qp_max = _cfg.qp_max;
  if (kind == PIC_RGB565) {
    hcfg.pic_type = ESP_H264_RAW_FMT_RGB565_LE;
  } else if (kind == PIC_UYVY) {
    hcfg.pic_type = ESP_H264_RAW_FMT_UYVY;
  } else if (kind == PIC_YUYV) {
    hcfg.pic_type = ESP_H264_RAW_FMT_YUYV;
  } else {
    hcfg.pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY;
  }

  esp_h264_enc_handle_t enc = nullptr;
  if (esp_h264_enc_hw_new(&hcfg, &enc) != ESP_H264_ERR_OK || !enc) return false;
  if (esp_h264_enc_open(enc) != ESP_H264_ERR_OK) {
    esp_h264_enc_del(enc);
    return false;
  }
  _enc = enc;
  _use_rgb565 = (kind == PIC_RGB565);
  _pic = kind;
  _opened = true;
  esp_h264_enc_param_hw_handle_t phd = nullptr;
  if (esp_h264_enc_hw_get_param_hd(enc, &phd) == ESP_H264_ERR_OK) _param = phd;
  return true;
}

bool ESP32P4_H264::ensureEncoder(PicKind kind) {
  if (kind == PIC_NONE) return false;
  if (_enc && _pic == kind && _opened) return true;
  dropEncoder();
  if (kind == PIC_RGB565) {
    if (createEncoder(PIC_RGB565)) return ensureBuffers();
    Serial.println("H264: RGB565_LE not accepted, using YUV packed");
    if (!createEncoder(PIC_YUV_PACKED)) return false;
    return ensureBuffers();
  }
  if (createEncoder(kind)) return ensureBuffers();
  if (kind == PIC_UYVY) {
    Serial.println("H264: UYVY not accepted, packing to YUV420");
    if (!createEncoder(PIC_YUV_PACKED)) return false;
    return ensureBuffers();
  }
  if (kind == PIC_YUYV) {
    Serial.println("H264: YUYV not accepted, packing to YUV420");
    if (!createEncoder(PIC_YUV_PACKED)) return false;
    return ensureBuffers();
  }
  return false;
}

bool ESP32P4_H264::setBitrate(uint32_t bps) {
  if (bps) _cfg.bitrate = bps;
  if (!_param) return _cfg.bitrate != 0;
  return esp_h264_enc_set_bitrate((esp_h264_enc_param_handle_t)_param, _cfg.bitrate) ==
         ESP_H264_ERR_OK;
}

bool ESP32P4_H264::setGop(uint8_t gop) {
  if (!gop) gop = 1;
  _cfg.gop = gop;
  if (!_param) return true;
  return esp_h264_enc_set_gop((esp_h264_enc_param_handle_t)_param, _cfg.gop) == ESP_H264_ERR_OK;
}

bool ESP32P4_H264::setFps(uint8_t fps) {
  if (!fps) fps = 1;
  _cfg.fps = fps;
  if (!_param) return true;
  return esp_h264_enc_set_fps((esp_h264_enc_param_handle_t)_param, _cfg.fps) == ESP_H264_ERR_OK;
}

bool ESP32P4_H264::setQp(uint8_t qp_min, uint8_t qp_max) {
  if (qp_min > 51) qp_min = 26;
  if (qp_max > 51 || qp_max < qp_min) qp_max = 36;
  _cfg.qp_min = qp_min;
  _cfg.qp_max = qp_max;
  if (!_param) return true;
  uint8_t mid = (uint8_t)(((unsigned)_cfg.qp_min + _cfg.qp_max) >> 1);
  return esp_h264_enc_hw_set_qp((esp_h264_enc_param_hw_handle_t)_param, mid) == ESP_H264_ERR_OK;
}

bool ESP32P4_H264::forceIdr() {
  if (!_param) return false;
  return esp_h264_enc_force_idr((esp_h264_enc_param_handle_t)_param) == ESP_H264_ERR_OK;
}

bool ESP32P4_H264::begin(const esp32p4_h264_cfg_t &cfg) {
  end();
  _cfg = cfg;
  _cfg.width = (uint16_t)((_cfg.width + 15) & ~15u);
  _cfg.height = (uint16_t)((_cfg.height + 15) & ~15u);
  if (!_cfg.fps) _cfg.fps = 15;
  if (!_cfg.gop) _cfg.gop = _cfg.fps;
  if (!_cfg.bitrate) _cfg.bitrate = (uint32_t)_cfg.width * _cfg.height * _cfg.fps / 8;
  if (_cfg.qp_min > 51) _cfg.qp_min = 26;
  if (_cfg.qp_max > 51 || _cfg.qp_max < _cfg.qp_min) _cfg.qp_max = 36;

  PicKind want = picKindFor(_cfg.input_format);
  if (!ensureEncoder(want)) {
    Serial.println("H264: esp_h264_enc_hw_new/open failed");
    return false;
  }

  _frames = 0;
  _pts = 0;
  Serial.printf("H264: HW %ux%u @ %ufps  br=%u  input=%s\n", _cfg.width, _cfg.height,
                (unsigned)_cfg.fps, (unsigned)_cfg.bitrate, inputName());
  return true;
}

bool ESP32P4_H264::ensureBuffers() {
  size_t nal_bytes = (size_t)_cfg.width * _cfg.height * 2;
  if (!_nal || _nal_cap < nal_bytes) {
    if (_nal) {
      heap_caps_free(_nal);
      _nal = nullptr;
      _nal_cap = 0;
    }
    uint32_t nal_need = 0;
    _nal = (uint8_t *)h264_aligned_alloc(nal_bytes, &nal_need);
    _nal_cap = nal_need;
    if (!_nal) {
      Serial.println("H264: NAL buffer alloc failed");
      return false;
    }
  }

  if (_pic == PIC_RGB565) {
    if (_yuv) {
      heap_caps_free(_yuv);
      _yuv = nullptr;
      _yuv_cap = 0;
    }
    size_t rgb_bytes = (size_t)_cfg.width * _cfg.height * 2;
    if (!_rgb_copy || _rgb_cap < rgb_bytes) {
      if (_rgb_copy) {
        heap_caps_free(_rgb_copy);
        _rgb_copy = nullptr;
        _rgb_cap = 0;
      }
      uint32_t rgb_need = 0;
      _rgb_copy = (uint8_t *)h264_aligned_alloc(rgb_bytes, &rgb_need);
      _rgb_cap = rgb_need;
      if (!_rgb_copy) {
        Serial.println("H264: RGB buffer alloc failed");
        return false;
      }
    }
  } else {
    if (_rgb_copy) {
      heap_caps_free(_rgb_copy);
      _rgb_copy = nullptr;
      _rgb_cap = 0;
    }
    size_t yuv_bytes = (_pic == PIC_UYVY || _pic == PIC_YUYV)
                           ? ((size_t)_cfg.width * _cfg.height * 2)
                           : ((size_t)_cfg.width * _cfg.height * 3 / 2);
    if (!_yuv || _yuv_cap < yuv_bytes) {
      if (_yuv) {
        heap_caps_free(_yuv);
        _yuv = nullptr;
        _yuv_cap = 0;
      }
      uint32_t yuv_need = 0;
      _yuv = (uint8_t *)h264_aligned_alloc(yuv_bytes, &yuv_need);
      _yuv_cap = yuv_need;
      if (!_yuv) {
        Serial.println("H264: YUV buffer alloc failed");
        return false;
      }
    }
  }
  return true;
}

void ESP32P4_H264::end() {
  closeFile();
  dropEncoder();
  if (_yuv) {
    heap_caps_free(_yuv);
    _yuv = nullptr;
    _yuv_cap = 0;
  }
  if (_nal) {
    heap_caps_free(_nal);
    _nal = nullptr;
    _nal_cap = 0;
  }
  if (_rgb_copy) {
    heap_caps_free(_rgb_copy);
    _rgb_copy = nullptr;
    _rgb_cap = 0;
  }
}

bool ESP32P4_H264::convertRgb565ToHwYuv(const uint8_t *rgb565, uint16_t w, uint16_t h) {
  if (!_yuv || !rgb565) return false;
  const uint16_t *src = (const uint16_t *)rgb565;
  const uint16_t dw = _cfg.width;
  const uint16_t dh = _cfg.height;
  uint8_t *dst = _yuv;
  size_t off = 0;

  // Fast path: same size, no resampling
  if (w == dw && h == dh) {
    for (uint16_t y = 0; y < dh; y++) {
      const bool even = (y & 1) == 0;
      const uint16_t *row = src + (size_t)y * w;
      for (uint16_t x = 0; x < dw; x += 2) {
        uint16_t p0 = row[x];
        uint16_t p1 = row[x + 1];
        uint8_t y0, u0, v0, y1, u1, v1;
        rgb_to_yuv(rgb565_r(p0), rgb565_g(p0), rgb565_b(p0), &y0, &u0, &v0);
        rgb_to_yuv(rgb565_r(p1), rgb565_g(p1), rgb565_b(p1), &y1, &u1, &v1);
        dst[off++] = even ? (uint8_t)(((unsigned)u0 + u1) >> 1) : (uint8_t)(((unsigned)v0 + v1) >> 1);
        dst[off++] = y0;
        dst[off++] = y1;
      }
    }
    return true;
  }

  for (uint16_t y = 0; y < dh; y++) {
    uint16_t sy = (h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (dh > 1 ? (dh - 1) : 1)) : 0;
    if (sy >= h) sy = h - 1;
    const bool even = (y & 1) == 0;
    for (uint16_t x = 0; x < dw; x += 2) {
      uint16_t sx0 = (w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0;
      uint16_t sx1 = (w > 1) ? (uint16_t)((uint32_t)(x + 1) * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0;
      if (sx0 >= w) sx0 = w - 1;
      if (sx1 >= w) sx1 = w - 1;
      uint16_t p0 = src[sy * w + sx0];
      uint16_t p1 = src[sy * w + sx1];
      uint8_t y0, u0, v0, y1, u1, v1;
      rgb_to_yuv(rgb565_r(p0), rgb565_g(p0), rgb565_b(p0), &y0, &u0, &v0);
      rgb_to_yuv(rgb565_r(p1), rgb565_g(p1), rgb565_b(p1), &y1, &u1, &v1);
      dst[off++] = even ? (uint8_t)(((unsigned)u0 + u1) >> 1) : (uint8_t)(((unsigned)v0 + v1) >> 1);
      dst[off++] = y0;
      dst[off++] = y1;
    }
  }
  return true;
}

static uint8_t i420_y(const uint8_t *i420, uint16_t w, uint16_t h, uint16_t x, uint16_t y) {
  if (x >= w) x = w - 1;
  if (y >= h) y = h - 1;
  return i420[(size_t)y * w + x];
}

static uint8_t i420_u(const uint8_t *i420, uint16_t w, uint16_t h, uint16_t x, uint16_t y) {
  const size_t ys = (size_t)w * h;
  uint16_t cx = x / 2, cy = y / 2;
  uint16_t cw = w / 2, ch = h / 2;
  if (!cw) cw = 1;
  if (!ch) ch = 1;
  if (cx >= cw) cx = cw - 1;
  if (cy >= ch) cy = ch - 1;
  return i420[ys + (size_t)cy * cw + cx];
}

static uint8_t i420_v(const uint8_t *i420, uint16_t w, uint16_t h, uint16_t x, uint16_t y) {
  const size_t ys = (size_t)w * h;
  const size_t us = ys / 4;
  uint16_t cx = x / 2, cy = y / 2;
  uint16_t cw = w / 2, ch = h / 2;
  if (!cw) cw = 1;
  if (!ch) ch = 1;
  if (cx >= cw) cx = cw - 1;
  if (cy >= ch) cy = ch - 1;
  return i420[ys + us + (size_t)cy * cw + cx];
}

bool ESP32P4_H264::convertI420ToHwYuv(const uint8_t *i420, uint16_t w, uint16_t h) {
  if (!_yuv || !i420 || !w || !h) return false;
  const uint16_t dw = _cfg.width;
  const uint16_t dh = _cfg.height;
  uint8_t *dst = _yuv;
  size_t off = 0;
  for (uint16_t y = 0; y < dh; y++) {
    uint16_t sy = (h == dh) ? y : ((h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (dh > 1 ? (dh - 1) : 1)) : 0);
    const bool even = (y & 1) == 0;
    for (uint16_t x = 0; x < dw; x += 2) {
      uint16_t sx0 = (w == dw) ? x : ((w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0);
      uint16_t sx1 = (w == dw) ? (uint16_t)(x + 1)
                               : ((w > 1) ? (uint16_t)((uint32_t)(x + 1) * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0);
      dst[off++] = even ? i420_u(i420, w, h, sx0, sy) : i420_v(i420, w, h, sx0, sy);
      dst[off++] = i420_y(i420, w, h, sx0, sy);
      dst[off++] = i420_y(i420, w, h, sx1, sy);
    }
  }
  return true;
}

bool ESP32P4_H264::convertGrayToHwYuv(const uint8_t *gray, uint16_t w, uint16_t h) {
  if (!_yuv || !gray || !w || !h) return false;
  const uint16_t dw = _cfg.width;
  const uint16_t dh = _cfg.height;
  uint8_t *dst = _yuv;
  size_t off = 0;
  for (uint16_t y = 0; y < dh; y++) {
    uint16_t sy = (h == dh) ? y : ((h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (dh > 1 ? (dh - 1) : 1)) : 0);
    if (sy >= h) sy = h - 1;
    const uint8_t *row = gray + (size_t)sy * w;
    for (uint16_t x = 0; x < dw; x += 2) {
      uint16_t sx0 = (w == dw) ? x : ((w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0);
      uint16_t sx1 = (w == dw) ? (uint16_t)(x + 1)
                               : ((w > 1) ? (uint16_t)((uint32_t)(x + 1) * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0);
      if (sx0 >= w) sx0 = w - 1;
      if (sx1 >= w) sx1 = w - 1;
      dst[off++] = 128;
      dst[off++] = row[sx0];
      dst[off++] = row[sx1];
    }
  }
  return true;
}

bool ESP32P4_H264::convertUyvy(const uint8_t *uyvy, uint16_t w, uint16_t h) {
  if (!uyvy || !w || !h) return false;
  const uint16_t dw = _cfg.width;
  const uint16_t dh = _cfg.height;
  if (_pic == PIC_UYVY) {
    if (!_yuv) return false;
    const size_t dst_stride = (size_t)dw * 2;
    if (w == dw && h == dh) {
      memcpy(_yuv, uyvy, dst_stride * dh);
      return true;
    }
    for (uint16_t y = 0; y < dh; y++) {
      uint16_t sy = (h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (dh > 1 ? (dh - 1) : 1)) : 0;
      if (sy >= h) sy = h - 1;
      const uint8_t *srow = uyvy + (size_t)sy * w * 2;
      uint8_t *drow = _yuv + (size_t)y * dst_stride;
      for (uint16_t x = 0; x < dw; x++) {
        uint16_t sx = (w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0;
        if (sx >= w) sx = w - 1;
        drow[x * 2] = srow[sx * 2];
        drow[x * 2 + 1] = srow[sx * 2 + 1];
      }
    }
    return true;
  }
  // Packed YUV420 fallback from UYVY (U Y0 V Y1)
  if (!_yuv) return false;
  uint8_t *dst = _yuv;
  size_t off = 0;
  for (uint16_t y = 0; y < dh; y++) {
    uint16_t sy = (h == dh) ? y : ((h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (dh > 1 ? (dh - 1) : 1)) : 0);
    if (sy >= h) sy = h - 1;
    const uint8_t *srow = uyvy + (size_t)sy * w * 2;
    const bool even = (y & 1) == 0;
    for (uint16_t x = 0; x < dw; x += 2) {
      uint16_t sx = (w == dw) ? x : ((w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0);
      if (sx & 1) sx = (uint16_t)(sx - 1);
      if (sx + 1 >= w) sx = (w >= 2) ? (uint16_t)(w - 2) : 0;
      const uint8_t *p = srow + (size_t)sx * 2;
      dst[off++] = even ? p[0] : p[2];
      dst[off++] = p[1];
      dst[off++] = p[3];
    }
  }
  return true;
}

bool ESP32P4_H264::convertYuyv(const uint8_t *yuyv, uint16_t w, uint16_t h) {
  if (!yuyv || !w || !h) return false;
  const uint16_t dw = _cfg.width;
  const uint16_t dh = _cfg.height;
  if (_pic == PIC_YUYV) {
    if (!_yuv) return false;
    const size_t dst_stride = (size_t)dw * 2;
    if (w == dw && h == dh) {
      memcpy(_yuv, yuyv, dst_stride * dh);
      return true;
    }
    for (uint16_t y = 0; y < dh; y++) {
      uint16_t sy = (h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (dh > 1 ? (dh - 1) : 1)) : 0;
      if (sy >= h) sy = h - 1;
      const uint8_t *srow = yuyv + (size_t)sy * w * 2;
      uint8_t *drow = _yuv + (size_t)y * dst_stride;
      for (uint16_t x = 0; x < dw; x++) {
        uint16_t sx = (w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0;
        if (sx >= w) sx = w - 1;
        drow[x * 2] = srow[sx * 2];
        drow[x * 2 + 1] = srow[sx * 2 + 1];
      }
    }
    return true;
  }
  /* Packed YUV420 fallback from YUYV (Y0 U Y1 V) */
  if (!_yuv) return false;
  uint8_t *dst = _yuv;
  size_t off = 0;
  for (uint16_t y = 0; y < dh; y++) {
    uint16_t sy = (h == dh) ? y : ((h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (dh > 1 ? (dh - 1) : 1)) : 0);
    if (sy >= h) sy = h - 1;
    const uint8_t *srow = yuyv + (size_t)sy * w * 2;
    const bool even = (y & 1) == 0;
    for (uint16_t x = 0; x < dw; x += 2) {
      uint16_t sx = (w == dw) ? x : ((w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (dw > 1 ? (dw - 1) : 1)) : 0);
      if (sx & 1) sx = (uint16_t)(sx - 1);
      if (sx + 1 >= w) sx = (w >= 2) ? (uint16_t)(w - 2) : 0;
      const uint8_t *p = srow + (size_t)sx * 2;
      dst[off++] = even ? p[1] : p[3];
      dst[off++] = p[0];
      dst[off++] = p[2];
    }
  }
  return true;
}

size_t ESP32P4_H264::submitFrame(const uint8_t *buf, uint32_t len, uint8_t *out, size_t out_cap,
                                 int *frame_type) {
  if (!_opened || !_enc || !buf || !len) return 0;
  esp_cache_msync((void *)buf, len, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

  esp_h264_enc_in_frame_t in = {};
  in.pts = _pts;
  in.raw_data.buffer = (uint8_t *)buf;
  in.raw_data.len = len;

  esp_h264_enc_out_frame_t outf = {};
  outf.raw_data.buffer = _nal;
  outf.raw_data.len = _nal_cap;

  esp_h264_err_t err = esp_h264_enc_process((esp_h264_enc_handle_t)_enc, &in, &outf);
  if (err != ESP_H264_ERR_OK) {
    Serial.printf("H264: process err=%d\n", (int)err);
    return 0;
  }
  if (!outf.length) return 0;

  if (out && out_cap) {
    if (outf.length > out_cap) return 0;
    memcpy(out, _nal, outf.length);
  }
  if (frame_type) *frame_type = (int)outf.frame_type;
  _frames++;
  _pts += (90000u / (_cfg.fps ? _cfg.fps : 15));
  return outf.length;
}

size_t ESP32P4_H264::processFrame(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out,
                                   size_t out_cap, int *frame_type) {
  if (!_opened || !_enc || !rgb565) return 0;

  if (_pic == PIC_RGB565 && _use_rgb565) {
    const size_t need = (size_t)_cfg.width * _cfg.height * 2;
    if (w == _cfg.width && h == _cfg.height) {
      memcpy(_rgb_copy, rgb565, need);
    } else {
      const uint16_t *src = (const uint16_t *)rgb565;
      uint16_t *dst = (uint16_t *)_rgb_copy;
      for (uint16_t y = 0; y < _cfg.height; y++) {
        uint16_t sy = (h > 1) ? (uint16_t)((uint32_t)y * (h - 1) / (_cfg.height > 1 ? (_cfg.height - 1) : 1)) : 0;
        for (uint16_t x = 0; x < _cfg.width; x++) {
          uint16_t sx = (w > 1) ? (uint16_t)((uint32_t)x * (w - 1) / (_cfg.width > 1 ? (_cfg.width - 1) : 1)) : 0;
          dst[y * _cfg.width + x] = src[sy * w + sx];
        }
      }
    }
    return submitFrame(_rgb_copy, (uint32_t)need, out, out_cap, frame_type);
  }
  if (_pic != PIC_YUV_PACKED) {
    if (!ensureEncoder(PIC_YUV_PACKED)) return 0;
  }
  if (!convertRgb565ToHwYuv(rgb565, w, h)) return 0;
  const size_t need = (size_t)_cfg.width * _cfg.height * 3 / 2;
  return submitFrame(_yuv, (uint32_t)need, out, out_cap, frame_type);
}

size_t ESP32P4_H264::encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap, int *frame_type) {
  if (!fb || !fb->buf) return 0;
  switch (fb->format) {
    case ESP32P4_PIXFORMAT_RGB565:
      return encode(fb->buf, fb->width, fb->height, out, out_cap, frame_type);
    case ESP32P4_PIXFORMAT_YUV420:
      if (!ensureEncoder(PIC_YUV_PACKED)) return 0;
      if (!convertI420ToHwYuv(fb->buf, fb->width, fb->height)) return 0;
      return submitFrame(_yuv, (uint32_t)((size_t)_cfg.width * _cfg.height * 3 / 2), out, out_cap,
                         frame_type);
    case ESP32P4_PIXFORMAT_GRAY8:
      if (!ensureEncoder(PIC_YUV_PACKED)) return 0;
      if (!convertGrayToHwYuv(fb->buf, fb->width, fb->height)) return 0;
      return submitFrame(_yuv, (uint32_t)((size_t)_cfg.width * _cfg.height * 3 / 2), out, out_cap,
                         frame_type);
    case ESP32P4_PIXFORMAT_YUV422:
      if (!ensureEncoder(PIC_UYVY)) return 0;
      if (!convertUyvy(fb->buf, fb->width, fb->height)) return 0;
      if (_pic == PIC_UYVY) {
        return submitFrame(_yuv, (uint32_t)((size_t)_cfg.width * _cfg.height * 2), out, out_cap,
                           frame_type);
      }
      return submitFrame(_yuv, (uint32_t)((size_t)_cfg.width * _cfg.height * 3 / 2), out, out_cap,
                         frame_type);
    case ESP32P4_PIXFORMAT_YUYV:
      if (!ensureEncoder(PIC_YUYV)) return 0;
      if (!convertYuyv(fb->buf, fb->width, fb->height)) return 0;
      if (_pic == PIC_YUYV) {
        return submitFrame(_yuv, (uint32_t)((size_t)_cfg.width * _cfg.height * 2), out, out_cap,
                           frame_type);
      }
      return submitFrame(_yuv, (uint32_t)((size_t)_cfg.width * _cfg.height * 3 / 2), out, out_cap,
                         frame_type);
    default:
      return 0;
  }
}

size_t ESP32P4_H264::encode(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out,
                            size_t out_cap, int *frame_type) {
  return processFrame(rgb565, w, h, out, out_cap, frame_type);
}

bool ESP32P4_H264::endsWithIgnoreCase(const char *s, const char *suffix) const {
  if (!s || !suffix) return false;
  size_t n = strlen(s), m = strlen(suffix);
  if (m > n) return false;
  for (size_t i = 0; i < m; i++) {
    char a = s[n - m + i], b = suffix[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

static File h264_open_retry(fs::FS *fs, const char *path, const char *mode) {
  File f;
  if (!fs || !path) return f;
  for (int i = 0; i < 10; i++) {
    f = fs->open(path, mode);
    if (f) return f;
    vTaskDelay(pdMS_TO_TICKS(40));
  }
  return f;
}

bool ESP32P4_H264::openMp4(ESP32P4_Sd *sd, const char *mp4_path, const char *pcm_path,
                           uint32_t pcm_rate_hz) {
  return openMp4((sd && sd->mounted()) ? &sd->fs() : nullptr, mp4_path, pcm_path, pcm_rate_hz);
}

bool ESP32P4_H264::openMp4(fs::FS *fs, const char *mp4_path, const char *pcm_path,
                           uint32_t pcm_rate_hz) {
  closeFile();
  if (!fs || !mp4_path) return false;
  if (!endsWithIgnoreCase(mp4_path, ".mp4")) {
    Serial.println("H264: openMp4 path must end with .mp4");
    return false;
  }

  // Temp work file beside target (deleted after remux — never kept as .h264)
  strncpy(_file_path, mp4_path, sizeof(_file_path) - 1);
  _file_path[sizeof(_file_path) - 1] = '\0';
  strncpy(_tmp_path, mp4_path, sizeof(_tmp_path) - 1);
  _tmp_path[sizeof(_tmp_path) - 1] = '\0';
  size_t n = strlen(_tmp_path);
  if (n >= 4) {
    _tmp_path[n - 4] = '\0';
    strncat(_tmp_path, ".rec_work", sizeof(_tmp_path) - strlen(_tmp_path) - 1);
  } else {
    strncpy(_tmp_path, "/rec.rec_work", sizeof(_tmp_path) - 1);
  }

  _file = h264_open_retry(fs, _tmp_path, FILE_WRITE);
  if (!_file) {
    Serial.printf("H264: open temp %s failed\n", _tmp_path);
    _file_path[0] = '\0';
    _tmp_path[0] = '\0';
    return false;
  }
  _fs = fs;
  _file_bytes = 0;
  _frames = 0;
  _pts = 0;
  _rec_t0_ms = millis();
  _file_open = true;
  _mp4_mode = true;
  _mux_pct = 0;
  _pcm_path[0] = '\0';
  _pcm_rate = 0;
  _pcm_ram = nullptr;
  _pcm_ram_sz = 0;
  if (pcm_path && pcm_path[0]) {
    strncpy(_pcm_path, pcm_path, sizeof(_pcm_path) - 1);
    _pcm_path[sizeof(_pcm_path) - 1] = '\0';
    _pcm_rate = pcm_rate_hz ? pcm_rate_hz : 16000;
  }
  Serial.printf("H264: recording MP4 -> %s%s\n", _file_path, _pcm_path[0] ? " +AAC" : "");
  return true;
}

uint32_t ESP32P4_H264::recordElapsedMs() const {
  if (!_file_open) return 0;
  return millis() - _rec_t0_ms;
}

bool ESP32P4_H264::openFile(ESP32P4_Sd *sd, const char *path) {
  return openFile((sd && sd->mounted()) ? &sd->fs() : nullptr, path);
}

bool ESP32P4_H264::openFile(fs::FS *fs, const char *path) {
  closeFile();
  if (!fs || !path) return false;
  // Convenience: .mp4 paths go through openMp4
  if (endsWithIgnoreCase(path, ".mp4")) return openMp4(fs, path);

  _file = fs->open(path, FILE_WRITE);
  if (!_file) {
    Serial.printf("H264: open %s failed\n", path);
    return false;
  }
  _fs = fs;
  strncpy(_file_path, path, sizeof(_file_path) - 1);
  _file_path[sizeof(_file_path) - 1] = '\0';
  _tmp_path[0] = '\0';
  _file_bytes = 0;
  _frames = 0;
  _pts = 0;
  _rec_t0_ms = millis();
  _file_open = true;
  _mp4_mode = false;
  Serial.printf("H264: recording raw -> %s\n", _file_path);
  return true;
}

size_t ESP32P4_H264::encodeToFile(const camera_fb_t *fb) {
  if (!fb || !_file_open || !_file) return 0;
  size_t n = encode(fb, nullptr, 0, nullptr);
  if (!n) return 0;
  size_t wr = _file.write(_nal, n);
  if (wr != n) {
    Serial.printf("H264: SD write %u/%u (DMA/storage busy)\n", (unsigned)wr, (unsigned)n);
    return 0;
  }
  _file_bytes += wr;
  if ((_frames & 15u) == 0) _file.flush();
  return wr;
}

size_t ESP32P4_H264::encodeToFile(const uint8_t *rgb565, uint16_t w, uint16_t h) {
  if (!_file_open || !_file) return 0;
  size_t n = processFrame(rgb565, w, h, nullptr, 0, nullptr);
  if (!n) return 0;
  size_t wr = _file.write(_nal, n);
  if (wr != n) {
    Serial.printf("H264: SD write %u/%u (DMA/storage busy)\n", (unsigned)wr, (unsigned)n);
    return 0;
  }
  _file_bytes += wr;
  if ((_frames & 15u) == 0) _file.flush();
  return wr;
}

void ESP32P4_H264::closeFile() {
  if (!_file_open) {
    _fs = nullptr;
    _mp4_mode = false;
    _rec_t0_ms = 0;
    _file_path[0] = '\0';
    _tmp_path[0] = '\0';
    return;
  }

  _file.flush();
  _file.close();
  _file_open = false;

  const uint32_t duration_ms = _rec_t0_ms ? (millis() - _rec_t0_ms) : 1;

  if (_mp4_mode && _fs && _tmp_path[0] && _file_path[0]) {
    if (_frames < 1 || _file_bytes < 32) {
      Serial.printf("H264: skip mux (frames=%u bytes=%llu) - nothing encoded\n", (unsigned)_frames,
                    (unsigned long long)_file_bytes);
      _fs->remove(_tmp_path);
      _fs->remove(_file_path);
      _file_path[0] = '\0';
      _file_bytes = 0;
    } else {
    Serial.printf("H264: muxing %u frames (%.2fs wall, %llu annexb) -> %s%s\n", (unsigned)_frames,
                  duration_ms / 1000.0f, (unsigned long long)_file_bytes, _file_path,
                  (_pcm_ram_sz || _pcm_path[0]) ? " +AAC" : "");
    vTaskDelay(pdMS_TO_TICKS(50));
    _mux_pct = 1;
    bool ok = false;
    if (_pcm_ram && _pcm_ram_sz && _pcm_rate) {
      ok = esp32p4_h264_annexb_to_mp4(*_fs, _tmp_path, _file_path, _cfg.width, _cfg.height,
                                      duration_ms, nullptr, _pcm_rate, 1, _pcm_ram, _pcm_ram_sz,
                                      &_mux_pct);
    } else if (_pcm_path[0] && _pcm_rate) {
      ok = esp32p4_h264_annexb_to_mp4(*_fs, _tmp_path, _file_path, _cfg.width, _cfg.height,
                                      duration_ms, _pcm_path, _pcm_rate, 1, nullptr, 0, &_mux_pct);
    } else {
      ok = esp32p4_h264_annexb_to_mp4(*_fs, _tmp_path, _file_path, _cfg.width, _cfg.height,
                                      duration_ms, nullptr, 0, 1, nullptr, 0, &_mux_pct);
    }
    if (ok) {
      File f = _fs->open(_file_path, FILE_READ);
      _file_bytes = f ? f.size() : 0;
      if (f) f.close();
      if (_file_bytes < 64) {
        ok = false;
        Serial.println("H264: MP4 was empty after mux");
      }
    }
    if (ok) {
      _fs->remove(_tmp_path);
      if (_pcm_path[0]) {
        _fs->remove(_pcm_path);
        _pcm_path[0] = '\0';
        _pcm_rate = 0;
      }
      float sec = duration_ms / 1000.0f;
      float afps = sec > 0.001f ? (_frames / sec) : 0;
      Serial.printf("H264: saved %s  frames=%u  bytes=%llu  duration=%.2fs  avg_fps=%.1f\n",
                    _file_path, (unsigned)_frames, (unsigned long long)_file_bytes, sec, afps);
    } else {
      Serial.println("H264: MP4 mux failed — keeping .rec_work, removing empty .mp4");
      _fs->remove(_file_path);
      char tmp_mp4[80];
      snprintf(tmp_mp4, sizeof(tmp_mp4), "%s.tmp", _file_path);
      _fs->remove(tmp_mp4);
      _file_path[0] = '\0';
      _file_bytes = 0;
    }
    }
  } else {
    Serial.printf("H264: closed %s  frames=%u  bytes=%llu\n", _file_path, (unsigned)_frames,
                  (unsigned long long)_file_bytes);
  }

  _fs = nullptr;
  _mp4_mode = false;
  _rec_t0_ms = 0;
  _tmp_path[0] = '\0';
  _pcm_path[0] = '\0';
  _pcm_rate = 0;
  _pcm_ram = nullptr;
  _pcm_ram_sz = 0;
}

#else  // !ESP32P4_HAS_ESP_H264

esp32p4_h264_cfg_t esp32p4_h264_cfg_default(uint16_t w, uint16_t h) {
  esp32p4_h264_cfg_t c{};
  c.width = (uint16_t)((w + 15) & ~15u);
  c.height = (uint16_t)((h + 15) & ~15u);
  if (c.width < 80) c.width = 80;
  if (c.height < 80) c.height = 80;
  c.fps = 15;
  c.gop = 15;
  c.bitrate = 400000;
  c.qp_min = 24;
  c.qp_max = 36;
  c.input_format = ESP32P4_PIXFORMAT_RGB565;
  return c;
}

bool ESP32P4_H264::begin(uint16_t w, uint16_t h, uint8_t fps, uint32_t bitrate) {
  (void)w;
  (void)h;
  (void)fps;
  (void)bitrate;
  Serial.println("H264: not available on this target (ESP32-P4 + esp_h264 required)");
  return false;
}

bool ESP32P4_H264::begin(const esp32p4_h264_cfg_t &cfg) {
  (void)cfg;
  return begin(0, 0, 0, 0);
}

ESP32P4_H264::PicKind ESP32P4_H264::picKindFor(esp32p4_cam_pixformat_t) const { return PIC_NONE; }
const char *ESP32P4_H264::inputName() const { return "none"; }
void ESP32P4_H264::dropEncoder() {}
bool ESP32P4_H264::createEncoder(PicKind) { return false; }
bool ESP32P4_H264::ensureEncoder(PicKind) { return false; }
bool ESP32P4_H264::ensureBuffers() { return false; }
void ESP32P4_H264::end() { closeFile(); }
bool ESP32P4_H264::convertRgb565ToHwYuv(const uint8_t *, uint16_t, uint16_t) { return false; }
bool ESP32P4_H264::convertI420ToHwYuv(const uint8_t *, uint16_t, uint16_t) { return false; }
bool ESP32P4_H264::convertGrayToHwYuv(const uint8_t *, uint16_t, uint16_t) { return false; }
bool ESP32P4_H264::convertUyvy(const uint8_t *, uint16_t, uint16_t) { return false; }
bool ESP32P4_H264::convertYuyv(const uint8_t *, uint16_t, uint16_t) { return false; }
size_t ESP32P4_H264::submitFrame(const uint8_t *, uint32_t, uint8_t *, size_t, int *) { return 0; }
bool ESP32P4_H264::setBitrate(uint32_t) { return false; }
bool ESP32P4_H264::setGop(uint8_t) { return false; }
bool ESP32P4_H264::setFps(uint8_t) { return false; }
bool ESP32P4_H264::setQp(uint8_t, uint8_t) { return false; }
bool ESP32P4_H264::forceIdr() { return false; }

size_t ESP32P4_H264::processFrame(const uint8_t *, uint16_t, uint16_t, uint8_t *, size_t, int *) {
  return 0;
}

size_t ESP32P4_H264::encode(const camera_fb_t *, uint8_t *, size_t, int *) { return 0; }
size_t ESP32P4_H264::encode(const uint8_t *, uint16_t, uint16_t, uint8_t *, size_t, int *) {
  return 0;
}

bool ESP32P4_H264::endsWithIgnoreCase(const char *, const char *) const { return false; }
bool ESP32P4_H264::openMp4(fs::FS *, const char *, const char *, uint32_t) { return false; }
bool ESP32P4_H264::openMp4(ESP32P4_Sd *, const char *, const char *, uint32_t) { return false; }
bool ESP32P4_H264::openFile(fs::FS *, const char *) { return false; }
bool ESP32P4_H264::openFile(ESP32P4_Sd *, const char *) { return false; }
size_t ESP32P4_H264::encodeToFile(const camera_fb_t *) { return 0; }
size_t ESP32P4_H264::encodeToFile(const uint8_t *, uint16_t, uint16_t) { return 0; }
uint32_t ESP32P4_H264::recordElapsedMs() const { return 0; }

void ESP32P4_H264::closeFile() {
  if (_file_open) {
    _file.close();
    _file_open = false;
  }
  _fs = nullptr;
  _mp4_mode = false;
  _rec_t0_ms = 0;
  _file_path[0] = '\0';
  _tmp_path[0] = '\0';
}

#endif  // ESP32P4_HAS_ESP_H264
