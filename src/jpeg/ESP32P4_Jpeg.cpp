#include "jpeg/ESP32P4_Jpeg.h"

#include <stdint.h>
#include <string.h>

#include "driver/jpeg_decode.h"
#include "driver/jpeg_encode.h"
#include "esp_heap_caps.h"
#include "mem/ESP32P4_Psram.h"

#if !(defined(CONFIG_ESP_REV_MIN_FULL) && (CONFIG_ESP_REV_MIN_FULL < 300) && \
      defined(CONFIG_IDF_TARGET_ESP32P4))
#define ESP32P4_JPEG_HAS_YUV420_IN 1
#endif

static jpeg_down_sampling_type_t chroma_sub(esp32p4_jpeg_chroma_t chroma,
                                            jpeg_down_sampling_type_t auto_sub) {
  switch (chroma) {
    case ESP32P4_JPEG_CHROMA_YUV444:
      return JPEG_DOWN_SAMPLING_YUV444;
    case ESP32P4_JPEG_CHROMA_YUV422:
      return JPEG_DOWN_SAMPLING_YUV422;
    case ESP32P4_JPEG_CHROMA_YUV420:
      return JPEG_DOWN_SAMPLING_YUV420;
    default:
      return auto_sub;
  }
}

enum jpeg_layout_t : uint8_t { JPEG_LAYOUT_PACKED = 0, JPEG_LAYOUT_I420 = 1 };

static bool jpeg_src_map(esp32p4_cam_pixformat_t fmt, jpeg_enc_input_format_t *src_type,
                         jpeg_down_sampling_type_t *sub, uint8_t *bpp, jpeg_layout_t *layout) {
  *layout = JPEG_LAYOUT_PACKED;
  switch (fmt) {
    case ESP32P4_PIXFORMAT_RGB888:
      *src_type = JPEG_ENCODE_IN_FORMAT_RGB888;
      *sub = JPEG_DOWN_SAMPLING_YUV444;
      *bpp = 3;
      return true;
    case ESP32P4_PIXFORMAT_YUV422:
      *src_type = JPEG_ENCODE_IN_FORMAT_YUV422;
      *sub = JPEG_DOWN_SAMPLING_YUV422;
      *bpp = 2;
      return true;
    case ESP32P4_PIXFORMAT_YUYV:
      *src_type = JPEG_ENCODE_IN_FORMAT_YUV422;
      *sub = JPEG_DOWN_SAMPLING_YUV422;
      *bpp = 2;
      return true;
    case ESP32P4_PIXFORMAT_GRAY8:
      *src_type = JPEG_ENCODE_IN_FORMAT_GRAY;
      *sub = JPEG_DOWN_SAMPLING_GRAY;
      *bpp = 1;
      return true;
    case ESP32P4_PIXFORMAT_RGB565:
      *src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
      *sub = JPEG_DOWN_SAMPLING_YUV422;
      *bpp = 2;
      return true;
#if ESP32P4_JPEG_HAS_YUV420_IN
    case ESP32P4_PIXFORMAT_YUV420:
      *src_type = JPEG_ENCODE_IN_FORMAT_YUV420;
      *sub = JPEG_DOWN_SAMPLING_YUV420;
      *bpp = 0;
      *layout = JPEG_LAYOUT_I420;
      return true;
#endif
    default:
      return false;
  }
}

static size_t jpeg_plane_bytes(jpeg_layout_t layout, uint8_t bpp, uint16_t w, uint16_t h) {
  if (layout == JPEG_LAYOUT_I420) return (size_t)w * (size_t)h * 3u / 2u;
  return (size_t)w * (size_t)h * (size_t)bpp;
}

static bool jpeg_copy_i420(uint8_t *dst, uint16_t pw, uint16_t ph, const uint8_t *src, uint16_t w,
                           uint16_t h) {
  const size_t y_src = (size_t)w * h;
  const size_t u_src = y_src / 4;
  const size_t y_dst = (size_t)pw * ph;
  const size_t u_dst = y_dst / 4;
  memset(dst, 128, y_dst + 2 * u_dst);
  for (uint16_t y = 0; y < h && y < ph; y++) {
    memcpy(dst + (size_t)y * pw, src + (size_t)y * w, w);
  }
  const uint8_t *us = src + y_src;
  const uint8_t *vs = us + u_src;
  uint8_t *ud = dst + y_dst;
  uint8_t *vd = ud + u_dst;
  const uint16_t cw = w / 2, ch = h / 2, pcw = pw / 2, pch = (uint16_t)(ph / 2);
  for (uint16_t y = 0; y < ch && y < pch; y++) {
    memcpy(ud + (size_t)y * pcw, us + (size_t)y * cw, cw);
    memcpy(vd + (size_t)y * pcw, vs + (size_t)y * cw, cw);
  }
  return true;
}

bool ESP32P4_Jpeg::begin(uint16_t max_w, uint16_t max_h, uint8_t quality, uint8_t max_src_bpp) {
  end();
  _quality = quality;
  _max_w = max_w;
  _max_h = max_h;
  _last_w = 0;
  _last_h = 0;
  if (max_src_bpp < 1) max_src_bpp = 2;
  if (max_src_bpp > 3) max_src_bpp = 3;

  jpeg_encode_engine_cfg_t eng = {};
  eng.intr_priority = 0;
  eng.timeout_ms = 200;
  jpeg_encoder_handle_t enc = nullptr;
  if (jpeg_new_encoder_engine(&eng, &enc) != ESP_OK) return false;
  _enc = enc;

  jpeg_decode_engine_cfg_t deng = {};
  deng.intr_priority = 0;
  deng.timeout_ms = 200;
  jpeg_decoder_handle_t dec = nullptr;
  if (jpeg_new_decoder_engine(&deng, &dec) != ESP_OK) {
    jpeg_del_encoder_engine(enc);
    _enc = nullptr;
    return false;
  }
  _dec = dec;

  const size_t want = (size_t)max_w * (size_t)max_h * (size_t)max_src_bpp;
  jpeg_encode_memory_alloc_cfg_t in_cfg = {};
  in_cfg.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER;
  size_t got = 0;
  _in = (uint8_t *)jpeg_alloc_encoder_mem(want, &in_cfg, &got);
  _in_cap = got;
  /* P4 L2 is 128B. jpeg_alloc_encoder_mem is often 64B-aligned; msync() then fails. */
  if (_in && (((uintptr_t)_in & (ESP32P4_CACHE_ALIGN - 1)) != 0)) {
    heap_caps_free(_in);
    _in = nullptr;
    _in_cap = 0;
  }
  if (!_in) {
    _in = (uint8_t *)esp32p4_psram_alloc(want);
    _in_cap = want ? ((want + ESP32P4_CACHE_ALIGN - 1) & ~(size_t)(ESP32P4_CACHE_ALIGN - 1)) : 0;
  }
  if (_in) memset(_in, 0, _in_cap);
  return _in != nullptr;
}

void ESP32P4_Jpeg::end() {
  if (_enc) {
    jpeg_del_encoder_engine((jpeg_encoder_handle_t)_enc);
    _enc = nullptr;
  }
  if (_dec) {
    jpeg_del_decoder_engine((jpeg_decoder_handle_t)_dec);
    _dec = nullptr;
  }
  if (_in) {
    heap_caps_free(_in);
    _in = nullptr;
    _in_cap = 0;
  }
  _last_w = 0;
  _last_h = 0;
}

void ESP32P4_Jpeg::clearInput() {
  if (_in && _in_cap) memset(_in, 0, _in_cap);
  _last_w = 0;
  _last_h = 0;
}

size_t ESP32P4_Jpeg::encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap) {
  if (!fb || !fb->buf || !out) return 0;
  if (fb->format == ESP32P4_PIXFORMAT_JPEG) {
    if (fb->len == 0 || fb->len > out_cap) return 0;
    memcpy(out, fb->buf, fb->len);
    return fb->len;
  }
  return encode(fb->buf, fb->width, fb->height, fb->format, out, out_cap);
}

size_t ESP32P4_Jpeg::encode(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out,
                            size_t out_cap) {
  return encode(rgb565, w, h, ESP32P4_PIXFORMAT_RGB565, out, out_cap);
}

size_t ESP32P4_Jpeg::encode(const uint8_t *src, uint16_t w, uint16_t h, esp32p4_cam_pixformat_t fmt,
                            uint8_t *out, size_t out_cap) {
  if (!_enc || !src || !out || !w || !h) return 0;
  if (w > _max_w || h > _max_h) return 0;

  jpeg_enc_input_format_t src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
  jpeg_down_sampling_type_t sub = JPEG_DOWN_SAMPLING_YUV422;
  uint8_t bpp = 2;
  jpeg_layout_t layout = JPEG_LAYOUT_PACKED;
  if (!jpeg_src_map(fmt, &src_type, &sub, &bpp, &layout)) return 0;

  const bool rgb_in = (fmt == ESP32P4_PIXFORMAT_RGB565 || fmt == ESP32P4_PIXFORMAT_RGB888);
  if (rgb_in) sub = chroma_sub(_chroma, sub);
  else if (fmt == ESP32P4_PIXFORMAT_GRAY8) sub = JPEG_DOWN_SAMPLING_GRAY;

  uint16_t pw = (uint16_t)((w + 15u) & ~15u);
  uint16_t ph = (uint16_t)((h + 15u) & ~15u);
  if (layout == JPEG_LAYOUT_I420) {
    pw = (uint16_t)((w + 15u) & ~15u);
    ph = (uint16_t)((h + 15u) & ~15u);
  }
  if (pw > _max_w || ph > _max_h) return 0;

  const size_t pad_bytes = jpeg_plane_bytes(layout, bpp, pw, ph);
  if (pad_bytes > _in_cap) return 0;

  if (pw != _last_w || ph != _last_h) {
    memset(_in, 0, _in_cap);
    _last_w = pw;
    _last_h = ph;
  }

  if (layout == JPEG_LAYOUT_I420) {
    jpeg_copy_i420(_in, pw, ph, src, w, h);
  } else {
    const size_t src_stride = (size_t)w * (size_t)bpp;
    const size_t dst_stride = (size_t)pw * (size_t)bpp;
    if (pw == w && ph == h) {
      memcpy(_in, src, pad_bytes);
    } else {
      memset(_in, 0, pad_bytes);
      for (uint16_t y = 0; y < h; y++) {
        memcpy(_in + (size_t)y * dst_stride, src + (size_t)y * src_stride, src_stride);
      }
    }
  }

  if (fmt == ESP32P4_PIXFORMAT_YUYV) {
    /* HW JPEG YUV422 input is UYVY. */
    for (size_t i = 0; i + 3 < pad_bytes; i += 4) {
      const uint8_t y0 = _in[i], u = _in[i + 1], y1 = _in[i + 2], v = _in[i + 3];
      _in[i] = u;
      _in[i + 1] = y0;
      _in[i + 2] = v;
      _in[i + 3] = y1;
    }
  }

  jpeg_encode_cfg_t cfg = {};
  cfg.width = pw;
  cfg.height = ph;
  cfg.src_type = src_type;
  cfg.sub_sample = sub;
  cfg.image_quality = _quality;
  uint32_t jlen = 0;
  esp32p4_psram_writeback(_in, pad_bytes > _in_cap ? _in_cap : pad_bytes);
  if (jpeg_encoder_process((jpeg_encoder_handle_t)_enc, &cfg, _in, (uint32_t)pad_bytes, out,
                           out_cap, &jlen) != ESP_OK) {
    return 0;
  }
  if (!jlen) return 0;
  if (jlen >= 2 && !(out[jlen - 2] == 0xFF && out[jlen - 1] == 0xD9) && jlen + 2 <= out_cap) {
    out[jlen++] = 0xFF;
    out[jlen++] = 0xD9;
  }
  return jlen;
}

bool ESP32P4_Jpeg::decodeInfo(const uint8_t *jpg, size_t jpg_len, uint32_t *w, uint32_t *h) {
  jpeg_decode_picture_info_t info = {};
  if (jpeg_decoder_get_info(jpg, jpg_len, &info) != ESP_OK) return false;
  if (w) *w = info.width;
  if (h) *h = info.height;
  return true;
}

size_t ESP32P4_Jpeg::decode(const uint8_t *jpg, size_t jpg_len, uint8_t *rgb_out, size_t out_cap,
                            uint32_t *w, uint32_t *h) {
  if (!_dec || !jpg || !rgb_out) return 0;
  jpeg_decode_cfg_t cfg = {};
  cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
  cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
  cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
  uint32_t out_size = 0;
  if (jpeg_decoder_process((jpeg_decoder_handle_t)_dec, &cfg, jpg, jpg_len, rgb_out, out_cap,
                           &out_size) != ESP_OK) {
    return 0;
  }
  uint32_t iw = 0, ih = 0;
  decodeInfo(jpg, jpg_len, &iw, &ih);
  if (w) *w = iw;
  if (h) *h = ih;
  return out_size;
}
