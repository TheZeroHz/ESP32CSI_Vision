#include "h264/ESP32P4_H264.h"

#include <Arduino.h>
#include <string.h>

#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_h264_enc_single_hw.h"
#include "h264/ESP32P4_H264Mp4.h"
#include "mem/ESP32P4_Psram.h"

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

bool ESP32P4_H264::createEncoder(bool rgb565_le) {
  esp_h264_enc_cfg_hw_t hcfg = {};
  hcfg.gop = _cfg.gop;
  hcfg.fps = _cfg.fps;
  hcfg.res.width = _cfg.width;
  hcfg.res.height = _cfg.height;
  hcfg.rc.bitrate = _cfg.bitrate;
  hcfg.rc.qp_min = _cfg.qp_min;
  hcfg.rc.qp_max = _cfg.qp_max;
  hcfg.pic_type = rgb565_le ? ESP_H264_RAW_FMT_RGB565_LE : ESP_H264_RAW_FMT_O_UYY_E_VYY;

  esp_h264_enc_handle_t enc = nullptr;
  if (esp_h264_enc_hw_new(&hcfg, &enc) != ESP_H264_ERR_OK || !enc) return false;
  if (esp_h264_enc_open(enc) != ESP_H264_ERR_OK) {
    esp_h264_enc_del(enc);
    return false;
  }
  _enc = enc;
  _use_rgb565 = rgb565_le;
  return true;
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

  // Prefer RGB565_LE (no CPU color convert) — fall back to YUV path.
  if (!createEncoder(true)) {
    Serial.println("H264: RGB565_LE not accepted, using YUV convert path");
    if (!createEncoder(false)) {
      Serial.println("H264: esp_h264_enc_hw_new/open failed");
      return false;
    }
  }

  if (!ensureBuffers()) {
    end();
    return false;
  }

  _opened = true;
  _frames = 0;
  _pts = 0;
  Serial.printf("H264: HW %ux%u @ %ufps  br=%u  input=%s\n", _cfg.width, _cfg.height,
                (unsigned)_cfg.fps, (unsigned)_cfg.bitrate, _use_rgb565 ? "RGB565" : "YUV");
  return true;
}

bool ESP32P4_H264::ensureBuffers() {
  size_t nal_bytes = (size_t)_cfg.width * _cfg.height * 2;
  uint32_t nal_need = 0;
  _nal = (uint8_t *)h264_aligned_alloc(nal_bytes, &nal_need);
  _nal_cap = nal_need;
  if (!_nal) {
    Serial.println("H264: NAL buffer alloc failed");
    return false;
  }

  if (_use_rgb565) {
    size_t rgb_bytes = (size_t)_cfg.width * _cfg.height * 2;
    uint32_t rgb_need = 0;
    _rgb_copy = (uint8_t *)h264_aligned_alloc(rgb_bytes, &rgb_need);
    _rgb_cap = rgb_need;
    if (!_rgb_copy) {
      Serial.println("H264: RGB buffer alloc failed");
      return false;
    }
  } else {
    size_t yuv_bytes = (size_t)_cfg.width * _cfg.height * 3 / 2;
    uint32_t yuv_need = 0;
    _yuv = (uint8_t *)h264_aligned_alloc(yuv_bytes, &yuv_need);
    _yuv_cap = yuv_need;
    if (!_yuv) {
      Serial.println("H264: YUV buffer alloc failed");
      return false;
    }
  }
  return true;
}

void ESP32P4_H264::end() {
  closeFile();
  if (_enc) {
    auto enc = (esp_h264_enc_handle_t)_enc;
    if (_opened) esp_h264_enc_close(enc);
    esp_h264_enc_del(enc);
    _enc = nullptr;
  }
  _opened = false;
  _use_rgb565 = false;
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

size_t ESP32P4_H264::processFrame(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out,
                                   size_t out_cap, int *frame_type) {
  if (!_opened || !_enc || !rgb565) return 0;

  esp_h264_enc_in_frame_t in = {};
  in.pts = _pts;

  if (_use_rgb565) {
    const size_t need = (size_t)_cfg.width * _cfg.height * 2;
    if (w == _cfg.width && h == _cfg.height) {
      memcpy(_rgb_copy, rgb565, need);
    } else {
      // nearest-neighbor into encoder size
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
    esp_cache_msync(_rgb_copy, need, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    in.raw_data.buffer = _rgb_copy;
    in.raw_data.len = (uint32_t)need;
  } else {
    if (!convertRgb565ToHwYuv(rgb565, w, h)) return 0;
    const size_t need = (size_t)_cfg.width * _cfg.height * 3 / 2;
    esp_cache_msync(_yuv, need, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    in.raw_data.buffer = _yuv;
    in.raw_data.len = (uint32_t)need;
  }

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

size_t ESP32P4_H264::encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap, int *frame_type) {
  if (!fb) return 0;
  return encode(fb->buf, fb->width, fb->height, out, out_cap, frame_type);
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

bool ESP32P4_H264::openMp4(ESP32P4_Sd *sd, const char *mp4_path) {
  closeFile();
  if (!sd || !sd->mounted() || !mp4_path) return false;
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

  _file = sd->fs().open(_tmp_path, FILE_WRITE);
  if (!_file) {
    Serial.printf("H264: open temp %s failed\n", _tmp_path);
    _file_path[0] = '\0';
    _tmp_path[0] = '\0';
    return false;
  }
  _sd = sd;
  _file_bytes = 0;
  _frames = 0;
  _pts = 0;
  _rec_t0_ms = millis();
  _file_open = true;
  _mp4_mode = true;
  Serial.printf("H264: recording MP4 -> %s\n", _file_path);
  return true;
}

uint32_t ESP32P4_H264::recordElapsedMs() const {
  if (!_file_open) return 0;
  return millis() - _rec_t0_ms;
}

bool ESP32P4_H264::openFile(ESP32P4_Sd *sd, const char *path) {
  closeFile();
  if (!sd || !sd->mounted() || !path) return false;
  // Convenience: .mp4 paths go through openMp4
  if (endsWithIgnoreCase(path, ".mp4")) return openMp4(sd, path);

  _file = sd->fs().open(path, FILE_WRITE);
  if (!_file) {
    Serial.printf("H264: open %s failed\n", path);
    return false;
  }
  _sd = sd;
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
  if (!fb) return 0;
  return encodeToFile(fb->buf, fb->width, fb->height);
}

size_t ESP32P4_H264::encodeToFile(const uint8_t *rgb565, uint16_t w, uint16_t h) {
  if (!_file_open || !_file) return 0;
  size_t n = processFrame(rgb565, w, h, nullptr, 0, nullptr);
  if (!n) return 0;
  size_t wr = _file.write(_nal, n);
  if (wr != n) return 0;
  _file_bytes += wr;
  if ((_frames & 15u) == 0) _file.flush();
  return wr;
}

void ESP32P4_H264::closeFile() {
  if (!_file_open) {
    _sd = nullptr;
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

  if (_mp4_mode && _sd && _tmp_path[0] && _file_path[0]) {
    Serial.printf("H264: muxing %u frames (%.2fs wall) -> %s\n", (unsigned)_frames,
                  duration_ms / 1000.0f, _file_path);
    bool ok = esp32p4_h264_annexb_to_mp4(_sd->fs(), _tmp_path, _file_path, _cfg.width, _cfg.height,
                                         duration_ms);
    _sd->remove(_tmp_path);
    if (ok) {
      File f = _sd->fs().open(_file_path, FILE_READ);
      _file_bytes = f ? f.size() : 0;
      if (f) f.close();
      float sec = duration_ms / 1000.0f;
      float afps = sec > 0.001f ? (_frames / sec) : 0;
      Serial.printf("H264: saved %s  frames=%u  bytes=%llu  duration=%.2fs  avg_fps=%.1f\n",
                    _file_path, (unsigned)_frames, (unsigned long long)_file_bytes, sec, afps);
    } else {
      Serial.println("H264: MP4 mux failed (temp removed)");
      _file_path[0] = '\0';
      _file_bytes = 0;
    }
  } else {
    Serial.printf("H264: closed %s  frames=%u  bytes=%llu\n", _file_path, (unsigned)_frames,
                  (unsigned long long)_file_bytes);
  }

  _sd = nullptr;
  _mp4_mode = false;
  _rec_t0_ms = 0;
  _tmp_path[0] = '\0';
}
