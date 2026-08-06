#include "ppa/ESP32P4_Ppa.h"

#include "driver/ppa.h"
#include "esp_cache.h"

bool ESP32P4_Ppa::begin() {
  if (_client) return true;
  ppa_client_config_t cfg = {};
  cfg.oper_type = PPA_OPERATION_SRM;
  cfg.max_pending_trans_num = 1;
  ppa_client_handle_t c = nullptr;
  if (ppa_register_client(&cfg, &c) != ESP_OK) return false;
  _client = c;
  return true;
}

void ESP32P4_Ppa::end() {
  if (_client) {
    ppa_unregister_client((ppa_client_handle_t)_client);
    _client = nullptr;
  }
}

bool ESP32P4_Ppa::srm(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                      uint16_t dst_h, int rot90s, bool mx, bool my) {
  if (!begin() || !src || !dst) return false;
  size_t need = (size_t)dst_w * dst_h * 2;
  if (dst_cap < need) return false;

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

  if (ppa_do_scale_rotate_mirror((ppa_client_handle_t)_client, &op) != ESP_OK) return false;
  (void)esp_cache_msync(dst, need, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  return true;
}

bool ESP32P4_Ppa::scale(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w,
                        uint16_t dst_h) {
  return srm(src, dst, dst_cap, dst_w, dst_h, 0, false, false);
}

bool ESP32P4_Ppa::rotate90(const camera_fb_t *src, uint8_t *dst, size_t dst_cap) {
  if (!src) return false;
  return srm(src, dst, dst_cap, src->height, src->width, 1, false, false);
}

bool ESP32P4_Ppa::mirror(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, bool mx, bool my) {
  if (!src) return false;
  return srm(src, dst, dst_cap, src->width, src->height, 0, mx, my);
}
