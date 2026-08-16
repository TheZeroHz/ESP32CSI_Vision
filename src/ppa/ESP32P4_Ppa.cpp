#include "ppa/ESP32P4_Ppa.h"

#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"

#include <string.h>

ESP32P4_Ppa &ESP32P4_Ppa::cv() {
  static ESP32P4_Ppa inst;
  return inst;
}

bool ESP32P4_Ppa::ensureSrm() {
  if (_srm) return true;
  ppa_client_config_t cfg = {};
  cfg.oper_type = PPA_OPERATION_SRM;
  cfg.max_pending_trans_num = 1;
  ppa_client_handle_t c = nullptr;
  if (ppa_register_client(&cfg, &c) != ESP_OK) return false;
  _srm = c;
  // BT.601-ish luma weights (sum 256) for RGB→GRAY SRM — optional on some IDF builds.
  if (ppa_set_rgb2gray_formula(77, 150, 29) != ESP_OK) {
    // GRAY convert may be unsupported; scale still works.
  }
  return true;
}

bool ESP32P4_Ppa::ensureFill() {
  if (_fill) return true;
  ppa_client_config_t cfg = {};
  cfg.oper_type = PPA_OPERATION_FILL;
  cfg.max_pending_trans_num = 1;
  ppa_client_handle_t c = nullptr;
  if (ppa_register_client(&cfg, &c) != ESP_OK) return false;
  _fill = c;
  return true;
}

bool ESP32P4_Ppa::begin() { return ensureSrm(); }

void ESP32P4_Ppa::end() {
  if (_srm) {
    ppa_unregister_client((ppa_client_handle_t)_srm);
    _srm = nullptr;
  }
  if (_fill) {
    ppa_unregister_client((ppa_client_handle_t)_fill);
    _fill = nullptr;
  }
}

bool ESP32P4_Ppa::srmRaw(const void *src, int sw, int sh, int src_cm, void *dst, size_t dst_cap,
                         int dw, int dh, int dst_cm, float sx, float sy) {
  if (!ensureSrm() || !src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return false;

  size_t bpp_out = (dst_cm == (int)PPA_SRM_COLOR_MODE_GRAY8) ? 1u : 2u;
  size_t need = (size_t)dw * (size_t)dh * bpp_out;
  // PPA wants capacity aligned to cache line; oversize slightly if caller under-reported.
  if (dst_cap < need) return false;
  size_t cap = dst_cap;
  if (cap < need + 128) {
    // still ok if exact need
  }

  size_t src_bytes = (size_t)sw * (size_t)sh * ((src_cm == (int)PPA_SRM_COLOR_MODE_GRAY8) ? 1u : 2u);
  (void)esp_cache_msync((void *)src, src_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

  ppa_srm_oper_config_t op = {};
  op.in.buffer = src;
  op.in.pic_w = (uint32_t)sw;
  op.in.pic_h = (uint32_t)sh;
  op.in.block_w = (uint32_t)sw;
  op.in.block_h = (uint32_t)sh;
  op.in.block_offset_x = 0;
  op.in.block_offset_y = 0;
  op.in.srm_cm = (ppa_srm_color_mode_t)src_cm;

  op.out.buffer = dst;
  op.out.buffer_size = cap;
  op.out.pic_w = (uint32_t)dw;
  op.out.pic_h = (uint32_t)dh;
  op.out.block_offset_x = 0;
  op.out.block_offset_y = 0;
  op.out.srm_cm = (ppa_srm_color_mode_t)dst_cm;

  op.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = sx;
  op.scale_y = sy;
  op.mirror_x = false;
  op.mirror_y = false;
  op.mode = PPA_TRANS_MODE_BLOCKING;

  if (ppa_do_scale_rotate_mirror((ppa_client_handle_t)_srm, &op) != ESP_OK) return false;
  (void)esp_cache_msync(dst, need, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  return true;
}

bool ESP32P4_Ppa::srm(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                      uint16_t dst_h, int rot90s, bool mx, bool my) {
  if (!ensureSrm() || !src || !dst) return false;
  size_t need = (size_t)dst_w * dst_h * 2;
  if (dst_cap < need) return false;

  (void)esp_cache_msync(src->buf, (size_t)src->width * src->height * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

  ppa_srm_oper_config_t op = {};
  op.in.buffer = src->buf;
  op.in.pic_w = src->width;
  op.in.pic_h = src->height;
  op.in.block_w = src->width;
  op.in.block_h = src->height;
  op.in.block_offset_x = 0;
  op.in.block_offset_y = 0;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  op.out.buffer = dst;
  op.out.buffer_size = dst_cap;
  op.out.pic_w = dst_w;
  op.out.pic_h = dst_h;
  op.out.block_offset_x = 0;
  op.out.block_offset_y = 0;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  static const ppa_srm_rotation_angle_t kRot[] = {
      PPA_SRM_ROTATION_ANGLE_0, PPA_SRM_ROTATION_ANGLE_90, PPA_SRM_ROTATION_ANGLE_180,
      PPA_SRM_ROTATION_ANGLE_270};
  op.rotation_angle = kRot[rot90s & 3];
  op.scale_x = (float)dst_w / (float)src->width;
  op.scale_y = (float)dst_h / (float)src->height;
  if (rot90s & 1) {
    op.scale_x = (float)dst_w / (float)src->height;
    op.scale_y = (float)dst_h / (float)src->width;
  }
  op.mirror_x = mx;
  op.mirror_y = my;
  op.mode = PPA_TRANS_MODE_BLOCKING;

  if (ppa_do_scale_rotate_mirror((ppa_client_handle_t)_srm, &op) != ESP_OK) return false;
  (void)esp_cache_msync(dst, need, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  return true;
}

bool ESP32P4_Ppa::scale(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                        uint16_t dst_h) {
  return srm(src, dst, dst_cap, dst_w, dst_h, 0, false, false);
}

bool ESP32P4_Ppa::scaleFit(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                           uint16_t dst_h) {
  if (!ensureSrm() || !src || !src->buf || !dst || dst_w < 16 || dst_h < 16) return false;
  // PPA needs even geometry; ÷16 only required for JPEG MCU (padded later if needed).
  if ((dst_w & 1) || (dst_h & 1)) return false;
  size_t need = (size_t)dst_w * (size_t)dst_h * 2;
  if (dst_cap < need) return false;

  const int sw = (int)src->width;
  const int sh = (int)src->height;
  if (sw < 2 || sh < 2) return false;

  // RGB565 PPA wants even geometry; do NOT floor 1080→1072 (that skews scale and
  // leaves a dirty bottom band when combined with non-uniform sx/sy).
  int cw = sw & ~1;
  int ch = sh & ~1;
  int ox = ((sw - cw) / 2) & ~1;
  int oy = ((sh - ch) / 2) & ~1;

  // Uniform fit: same scale on X/Y, letterbox into cleared destination.
  float sx = (float)dst_w / (float)cw;
  float sy = (float)dst_h / (float)ch;
  float s = sx < sy ? sx : sy;
  int ow = ((int)(cw * s + 0.5f)) & ~1;
  int oh = ((int)(ch * s + 0.5f)) & ~1;
  if (ow < 2) ow = 2;
  if (oh < 2) oh = 2;
  if (ow > (int)dst_w) ow = (int)dst_w & ~1;
  if (oh > (int)dst_h) oh = (int)dst_h & ~1;
  // Snap to ÷16 only when destination itself is MCU-aligned (normal stream sizes).
  const bool dst_mcu = ((dst_w % 16) == 0) && ((dst_h % 16) == 0);
  if (dst_mcu) {
    ow = (ow / 16) * 16;
    oh = (oh / 16) * 16;
    if (ow < 16) ow = 16;
    if (oh < 16) oh = 16;
  }
  if (ow > (int)dst_w) ow = (int)dst_w & ~1;
  if (oh > (int)dst_h) oh = (int)dst_h & ~1;

  int off_x = (((int)dst_w - ow) / 2) & ~1;
  int off_y = (((int)dst_h - oh) / 2) & ~1;

  memset(dst, 0, need);
  (void)esp_cache_msync(src->buf, (size_t)sw * (size_t)sh * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

  // Prefer exact reciprocal scales when possible (e.g. 1920→1280 = 2/3).
  float use_sx = (float)ow / (float)cw;
  float use_sy = (float)oh / (float)ch;

  ppa_srm_oper_config_t op = {};
  op.in.buffer = src->buf;
  op.in.pic_w = (uint32_t)sw;
  op.in.pic_h = (uint32_t)sh;
  op.in.block_w = (uint32_t)cw;
  op.in.block_h = (uint32_t)ch;
  op.in.block_offset_x = (uint32_t)ox;
  op.in.block_offset_y = (uint32_t)oy;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  op.out.buffer = dst;
  // Full allocation size — PPA needs cache-line headroom; exact `need` corrupted the bottom.
  op.out.buffer_size = dst_cap;
  op.out.pic_w = dst_w;
  op.out.pic_h = dst_h;
  op.out.block_offset_x = (uint32_t)off_x;
  op.out.block_offset_y = (uint32_t)off_y;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  op.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = use_sx;
  op.scale_y = use_sy;
  op.mirror_x = false;
  op.mirror_y = false;
  op.mode = PPA_TRANS_MODE_BLOCKING;

  if (ppa_do_scale_rotate_mirror((ppa_client_handle_t)_srm, &op) != ESP_OK) return false;

  size_t sync = need;
  const size_t cl = 64;
  sync = (sync + cl - 1) & ~(cl - 1);
  if (sync > dst_cap) sync = dst_cap;
  (void)esp_cache_msync(dst, sync, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  return true;
}

bool ESP32P4_Ppa::scaleCover(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                             uint16_t dst_h) {
  if (!ensureSrm() || !src || !src->buf || !dst || dst_w < 16 || dst_h < 16) return false;
  if ((dst_w & 1) || (dst_h & 1)) return false;
  size_t need = (size_t)dst_w * (size_t)dst_h * 2;
  if (dst_cap < need) return false;

  const int sw = (int)src->width;
  const int sh = (int)src->height;
  if (sw < 2 || sh < 2) return false;

  int cw = sw & ~1;
  int ch = sh & ~1;
  // Crop source to destination aspect (cover), then scale 1:1 to fill dst.
  const float tar = (float)dst_w / (float)dst_h;
  const float src_ar = (float)cw / (float)ch;
  int ox = 0, oy = 0;
  if (src_ar > tar) {
    int bw = ((int)((float)ch * tar + 0.5f)) & ~1;
    if (bw < 2) bw = 2;
    if (bw > cw) bw = cw;
    ox = ((cw - bw) / 2) & ~1;
    cw = bw;
  } else {
    int bh = ((int)((float)cw / tar + 0.5f)) & ~1;
    if (bh < 2) bh = 2;
    if (bh > ch) bh = ch;
    oy = ((ch - bh) / 2) & ~1;
    ch = bh;
  }

  memset(dst, 0, need);
  (void)esp_cache_msync(src->buf, (size_t)sw * (size_t)sh * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

  ppa_srm_oper_config_t op = {};
  op.in.buffer = src->buf;
  op.in.pic_w = (uint32_t)sw;
  op.in.pic_h = (uint32_t)sh;
  op.in.block_w = (uint32_t)cw;
  op.in.block_h = (uint32_t)ch;
  op.in.block_offset_x = (uint32_t)ox;
  op.in.block_offset_y = (uint32_t)oy;
  op.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  op.out.buffer = dst;
  op.out.buffer_size = dst_cap;
  op.out.pic_w = dst_w;
  op.out.pic_h = dst_h;
  op.out.block_offset_x = 0;
  op.out.block_offset_y = 0;
  op.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  op.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  op.scale_x = (float)dst_w / (float)cw;
  op.scale_y = (float)dst_h / (float)ch;
  op.mirror_x = false;
  op.mirror_y = false;
  op.mode = PPA_TRANS_MODE_BLOCKING;

  if (ppa_do_scale_rotate_mirror((ppa_client_handle_t)_srm, &op) != ESP_OK) return false;

  size_t sync = need;
  const size_t cl = 64;
  sync = (sync + cl - 1) & ~(cl - 1);
  if (sync > dst_cap) sync = dst_cap;
  (void)esp_cache_msync(dst, sync, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  return true;
}

bool ESP32P4_Ppa::rotate90(const camera_fb_t *src, uint8_t *dst, size_t dst_cap) {
  if (!src) return false;
  return srm(src, dst, dst_cap, src->height, src->width, 1, false, false);
}

bool ESP32P4_Ppa::mirror(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, bool mx, bool my) {
  if (!src) return false;
  return srm(src, dst, dst_cap, src->width, src->height, 0, mx, my);
}

bool ESP32P4_Ppa::scaleRgb565(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh) {
  size_t cap = (size_t)dw * (size_t)dh * 2;
  return srmRaw(src, sw, sh, (int)PPA_SRM_COLOR_MODE_RGB565, dst, cap, dw, dh,
                (int)PPA_SRM_COLOR_MODE_RGB565, (float)dw / (float)sw, (float)dh / (float)sh);
}

bool ESP32P4_Ppa::rgb565ToGray(const uint16_t *src, int w, int h, uint8_t *dst) {
  // Some ESP32-P4 / IDF builds reject RGB565→GRAY8 ("unsupported color mode").
  // Probe once; on failure callers use SW luma (Cv::toGray / Img::luma565).
  static int8_t s_gray8 = -1;
  if (s_gray8 == 0) return false;
  size_t cap = (size_t)w * (size_t)h;
  bool ok = srmRaw(src, w, h, (int)PPA_SRM_COLOR_MODE_RGB565, dst, cap, w, h,
                   (int)PPA_SRM_COLOR_MODE_GRAY8, 1.0f, 1.0f);
  s_gray8 = ok ? 1 : 0;
  return ok;
}

bool ESP32P4_Ppa::rgb565ToGrayScale(const uint16_t *src, int sw, int sh, uint8_t *dst, int dw,
                                    int dh) {
  if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return false;
  // Prefer one-shot GRAY8 when HW supports it.
  static int8_t s_gray8 = -1;
  if (s_gray8 != 0) {
    size_t cap = (size_t)dw * (size_t)dh;
    bool ok = srmRaw(src, sw, sh, (int)PPA_SRM_COLOR_MODE_RGB565, dst, cap, dw, dh,
                     (int)PPA_SRM_COLOR_MODE_GRAY8, (float)dw / (float)sw, (float)dh / (float)sh);
    if (ok) {
      s_gray8 = 1;
      return true;
    }
    s_gray8 = 0;
  }
  // HW path that works on all P4 builds: PPA RGB565 scale, then SW luma.
  size_t rgb_bytes = (size_t)dw * (size_t)dh * 2u;
  uint16_t *tmp = (uint16_t *)heap_caps_aligned_alloc(128, rgb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!tmp) tmp = (uint16_t *)heap_caps_aligned_alloc(128, rgb_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!tmp) return false;
  bool ok = scaleRgb565(src, sw, sh, tmp, dw, dh);
  if (ok) {
    size_t n = (size_t)dw * (size_t)dh;
    for (size_t i = 0; i < n; i++) {
      uint16_t px = tmp[i];
      uint32_t r = (px >> 11) & 0x1f;
      uint32_t g = (px >> 5) & 0x3f;
      uint32_t b = px & 0x1f;
      dst[i] = (uint8_t)((r * 38 + g * 75 + b * 15) >> 5);
    }
  }
  heap_caps_free(tmp);
  return ok;
}

bool ESP32P4_Ppa::fillRect565(uint16_t *img, int w, int h, int x, int y, int rw, int rh,
                              uint16_t color) {
  if (!ensureFill() || !img || w <= 0 || h <= 0 || rw <= 0 || rh <= 0) return false;
  if (x < 0) {
    rw += x;
    x = 0;
  }
  if (y < 0) {
    rh += y;
    y = 0;
  }
  if (x + rw > w) rw = w - x;
  if (y + rh > h) rh = h - y;
  if (rw <= 0 || rh <= 0) return false;

  size_t bytes = (size_t)w * (size_t)h * 2;
  (void)esp_cache_msync(img, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

  ppa_fill_oper_config_t op = {};
  op.out.buffer = img;
  op.out.buffer_size = bytes;
  op.out.pic_w = (uint32_t)w;
  op.out.pic_h = (uint32_t)h;
  op.out.block_offset_x = (uint32_t)x;
  op.out.block_offset_y = (uint32_t)y;
  op.out.fill_cm = PPA_FILL_COLOR_MODE_RGB565;
  op.fill_block_w = (uint32_t)rw;
  op.fill_block_h = (uint32_t)rh;
  op.fill_color_val = color;
  op.mode = PPA_TRANS_MODE_BLOCKING;

  if (ppa_do_fill((ppa_client_handle_t)_fill, &op) != ESP_OK) return false;
  (void)esp_cache_msync(img, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  return true;
}
