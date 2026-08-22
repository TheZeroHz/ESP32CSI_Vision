#include "cam/ESP32P4_Isp.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "cam/ESP32P4_Camera.h"
#include "cam/ESP32P4_Ipa.h"
#include "driver/isp_ae.h"
#include "driver/isp_awb.h"
#include "driver/isp_bf.h"
#include "driver/isp_ccm.h"
#include "driver/isp_color.h"
#include "driver/isp_demosaic.h"
#include "driver/isp_gamma.h"
#include "driver/isp_lsc.h"
#include "driver/isp_sharpen.h"
#if __has_include("driver/isp_hist.h")
#include "driver/isp_hist.h"
#endif
#if __has_include("driver/isp_blc.h")
#include "driver/isp_blc.h"
#endif
#if __has_include("driver/isp_af.h")
#include "driver/isp_af.h"
#endif
#include "esp_heap_caps.h"
#include "hal/isp_ll.h"

#ifndef ESP32P4_ISP_3A_MS
#define ESP32P4_ISP_3A_MS 80
#endif

static float s_gamma_e = 0.55f;

static uint32_t gamma_curve(uint32_t x) {
  if (x >= 256) return 256;
  float n = (float)x / 256.0f;
  float y = powf(n, s_gamma_e) * 256.0f;
  if (y > 255.0f) y = 255.0f;
  return (uint32_t)(y + 0.5f);
}

static void u8_to_fixed17(uint8_t v, uint32_t *integer, uint32_t *decimal) {
  if (v >= 128) {
    *integer = 1;
    *decimal = (uint32_t)(v - 128);
    if (*decimal > 127) *decimal = 127;
  } else {
    *integer = 0;
    *decimal = (uint32_t)v * 127 / 128;
  }
}

static void f_to_fixed(float v, uint32_t ibits, uint32_t dbits, uint32_t *i, uint32_t *d) {
  if (v < 0.f) v = 0.f;
  const uint32_t imax = (1u << ibits) - 1u;
  const uint32_t dmax = (1u << dbits);
  uint32_t ii = (uint32_t)v;
  uint32_t dd = (uint32_t)((v - (float)ii) * (float)dmax + 0.5f);
  if (dd >= dmax) {
    ii++;
    dd = 0;
  }
  if (ii > imax) {
    ii = imax;
    dd = dmax - 1u;
  }
  *i = ii;
  *d = dd;
}

static uint32_t lsc_grids(uint32_t res) {
  if (res < 2) return 2;
  return (res - 1u) / 2u / (uint32_t)ISP_LL_LSC_GRID_HEIGHT + 2u;
}

static float lsc_unpack(uint16_t p) {
  return (float)((p >> 8) & 3) + (float)(p & 0xff) / 256.f;
}

static isp_lsc_gain_t lsc_pack(float f) {
  isp_lsc_gain_t g = {};
  if (f < 0.f) f = 0.f;
  if (f > 3.996f) f = 3.996f;
  uint32_t i = (uint32_t)f;
  uint32_t d = (uint32_t)((f - (float)i) * 256.f + 0.5f);
  if (d >= 256) {
    i++;
    d = 0;
  }
  if (i > 3) {
    i = 3;
    d = 255;
  }
  g.integer = i;
  g.decimal = d;
  return g;
}

bool IRAM_ATTR ESP32P4_Isp::onAwb(isp_awb_ctlr_t, const esp_isp_awb_evt_data_t *edata, void *ctx) {
  auto *self = (ESP32P4_Isp *)ctx;
  if (!self || !edata) return false;
  self->_white_n = edata->awb_result.white_patch_num;
  self->_sum_r = edata->awb_result.sum_r;
  self->_sum_g = edata->awb_result.sum_g;
  self->_sum_b = edata->awb_result.sum_b;
#if ISP_AWB_WINDOW_X_NUM > 0 && ISP_AWB_WINDOW_Y_NUM > 0
  const isp_awb_subwin_stat_result_t &sw = edata->awb_result.subwin_result;
  for (int y = 0; y < ISP_AWB_WINDOW_Y_NUM; y++) {
    for (int x = 0; x < ISP_AWB_WINDOW_X_NUM; x++) {
      self->_sw_n[x][y] = sw.white_patch_num[x][y];
      self->_sw_r[x][y] = sw.sum_r[x][y];
      self->_sw_g[x][y] = sw.sum_g[x][y];
      self->_sw_b[x][y] = sw.sum_b[x][y];
    }
  }
#endif
  self->_awb_fresh = 1;
  return false;
}

bool IRAM_ATTR ESP32P4_Isp::onAe(isp_ae_ctlr_t, const esp_isp_ae_env_detector_evt_data_t *edata,
                                void *ctx) {
  auto *self = (ESP32P4_Isp *)ctx;
  if (!self || !edata) return false;
  for (int y = 0; y < ISP_AE_BLOCK_Y_NUM; y++) {
    for (int x = 0; x < ISP_AE_BLOCK_X_NUM; x++) {
      int v = edata->ae_result.luminance[x][y];
      if (v < 0) v = 0;
      if (v > 255) v = 255;
      self->_ae_luma[x][y] = (uint8_t)v;
    }
  }
  self->_ae_fresh = 1;
  return false;
}

#if __has_include("driver/isp_hist.h")
bool IRAM_ATTR ESP32P4_Isp::onHist(isp_hist_ctlr_t, const esp_isp_hist_evt_data_t *edata, void *ctx) {
  auto *self = (ESP32P4_Isp *)ctx;
  if (!self || !edata) return false;
  for (int i = 0; i < 16 && i < ISP_HIST_SEGMENT_NUMS; i++) {
    self->_hist_bin[i] = edata->hist_result.hist_value[i];
  }
  self->_hist_fresh = 1;
  return false;
}
#endif

bool ESP32P4_Isp::begin(isp_proc_handle_t proc, uint16_t w, uint16_t h,
                        esp32p4_cam_sensor_t sensor) {
  end();
  if (!proc || w < 16 || h < 16) return false;
  _proc = proc;
  _w = w;
  _h = h;
  _sensor = sensor;
  _ipa = esp32p4_ipa_profile(sensor);
  _rgain = 1.0f;
  _bgain = 1.0f;
  _ct = 5000.f;
  _manual_once = false;
  _lsc_idx = 0xffff;
  _blc_key = 0xffff;
  _blc_skip = false;
  _env_luma = 0;
  if (_ipa) {
    if (!_ae_user) _ae_target = _ipa->ae_target;
    _contrast = _ipa->contrast;
    _saturation = _ipa->sat;
    _gamma = _ipa->gamma;
    _denoise = _ipa->bf_level ? _ipa->bf_level : 4;
  }
  if (!startBlocks()) {
    end();
    return false;
  }
  (void)applyLsc(_ct);
  (void)applyBlc(1.f);
  return true;
}

void ESP32P4_Isp::end() {
  stopAe();
  stopAwb();
  stopHist();
  stopAf();
  freeLsc();
  if (_proc) {
#if __has_include("driver/isp_blc.h")
    if (_blc_on) (void)esp_isp_blc_disable(_proc);
#endif
    (void)esp_isp_sharpen_disable(_proc);
    (void)esp_isp_gamma_disable(_proc);
    (void)esp_isp_color_disable(_proc);
    (void)esp_isp_ccm_disable(_proc);
    (void)esp_isp_bf_disable(_proc);
  }
  _proc = nullptr;
  _ipa = nullptr;
  _blc_on = false;
  _blc_skip = false;
}

void ESP32P4_Isp::freeLsc() {
  if (_proc && _lsc_on) {
    (void)esp_isp_lsc_disable(_proc);
  }
  _lsc_on = false;
  if (_lsc_arr.gain_r) {
    heap_caps_free(_lsc_arr.gain_r);
    _lsc_arr.gain_r = nullptr;
  }
  if (_lsc_arr.gain_gr) {
    heap_caps_free(_lsc_arr.gain_gr);
    _lsc_arr.gain_gr = nullptr;
  }
  if (_lsc_arr.gain_gb) {
    heap_caps_free(_lsc_arr.gain_gb);
    _lsc_arr.gain_gb = nullptr;
  }
  if (_lsc_arr.gain_b) {
    heap_caps_free(_lsc_arr.gain_b);
    _lsc_arr.gain_b = nullptr;
  }
  _lsc_n = 0;
}

const char *ESP32P4_Isp::profileName() const { return _ipa && _ipa->name ? _ipa->name : "none"; }

bool ESP32P4_Isp::startBlocks() {
  if (!applyBf()) return false;

  if (_ipa) {
    esp_isp_demosaic_config_t dm = {};
    dm.padding_mode = ISP_DEMOSAIC_EDGE_PADDING_MODE_SRND_DATA;
    uint32_t gi = 1, gd = 8;
    f_to_fixed(_dm_grad, ISP_DEMOSAIC_GRAD_RATIO_INT_BITS, ISP_DEMOSAIC_GRAD_RATIO_DEC_BITS, &gi,
               &gd);
    dm.grad_ratio.integer = gi;
    dm.grad_ratio.decimal = gd;
    (void)esp_isp_demosaic_configure(_proc, &dm);
  }

  if (!applyCcm()) return false;
  if (esp_isp_ccm_enable(_proc) != ESP_OK) return false;

  if (!applyColor()) return false;
  if (esp_isp_color_enable(_proc) != ESP_OK) return false;

  if (!applyGamma()) {
    Serial.println("CSI: ISP gamma configure failed");
  }

  (void)applySharpen();

  if (_awb_on && !startAwb()) Serial.println("CSI: ISP AWB start failed (continuing)");
  if (_ae_on && !startAe()) Serial.println("CSI: ISP AE start failed (continuing)");
  if (!startHist()) Serial.println("CSI: ISP hist start failed (continuing)");
  return true;
}

bool ESP32P4_Isp::applyColor() {
  if (!_proc) return false;
  esp_isp_color_config_t c = {};
  uint32_t ci = 0, cd = 0, si = 0, sd = 0;
  u8_to_fixed17(_contrast, &ci, &cd);
  u8_to_fixed17(_saturation, &si, &sd);
  c.color_contrast.integer = ci;
  c.color_contrast.decimal = cd;
  c.color_saturation.integer = si;
  c.color_saturation.decimal = sd;
  c.color_hue = _hue > 360 ? 360 : _hue;
  c.color_brightness = _brightness;
  return esp_isp_color_configure(_proc, &c) == ESP_OK;
}

bool ESP32P4_Isp::applyGamma() {
  if (!_proc) return false;
  s_gamma_e = _gamma > 0.15f ? _gamma : 0.55f;
  isp_gamma_curve_points_t pts = {};
  if (esp_isp_gamma_fill_curve_points(gamma_curve, &pts) != ESP_OK) return false;
  if (esp_isp_gamma_configure(_proc, COLOR_COMPONENT_R, &pts) != ESP_OK) return false;
  if (esp_isp_gamma_configure(_proc, COLOR_COMPONENT_G, &pts) != ESP_OK) return false;
  if (esp_isp_gamma_configure(_proc, COLOR_COMPONENT_B, &pts) != ESP_OK) return false;
  return esp_isp_gamma_enable(_proc) == ESP_OK;
}

bool ESP32P4_Isp::applyCcm() {
  if (!_proc) return false;
  float m[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
  if (_ccm_user) {
    memcpy(m, _ccm_m, sizeof(m));
  } else {
    if (_ipa) esp32p4_ipa_interp_ccm(_ipa, _ct, _last_luma, m);
    const float rg = _rgain * _user_rgain;
    const float bg = _bgain * _user_bgain;
    m[0] *= rg;
    m[3] *= rg;
    m[6] *= rg;
    m[2] *= bg;
    m[5] *= bg;
    m[8] *= bg;
  }
  esp_isp_ccm_config_t ccm = {};
  ccm.saturation = true;
  ccm.matrix[0][0] = m[0];
  ccm.matrix[0][1] = m[1];
  ccm.matrix[0][2] = m[2];
  ccm.matrix[1][0] = m[3];
  ccm.matrix[1][1] = m[4];
  ccm.matrix[1][2] = m[5];
  ccm.matrix[2][0] = m[6];
  ccm.matrix[2][1] = m[7];
  ccm.matrix[2][2] = m[8];
  return esp_isp_ccm_configure(_proc, &ccm) == ESP_OK;
}

bool ESP32P4_Isp::applyLsc(float ct) {
  if (!_proc || !_ipa || !_ipa->lsc || !_ipa->lsc->ch) return false;
  const Esp32p4IpaLsc *L = _ipa->lsc;
  const uint16_t idx = esp32p4_ipa_nearest_lsc(L, ct);
  if (idx == _lsc_idx && _lsc_on) return true;

  if (!_lsc_arr.gain_r) {
    if (esp_isp_lsc_allocate_gain_array(_proc, &_lsc_arr, &_lsc_n) != ESP_OK || !_lsc_n) {
      Serial.println("CSI: ISP LSC alloc failed");
      return false;
    }
  }

  const uint32_t dw = lsc_grids(_w);
  const uint32_t dh = lsc_grids(_h);
  if (dw * dh != _lsc_n) {
    Serial.printf("CSI: ISP LSC grid mismatch %u*%u != %u\n", (unsigned)dw, (unsigned)dh,
                  (unsigned)_lsc_n);
    return false;
  }
  const uint32_t sw = lsc_grids(L->img_w);
  const uint32_t sh = lsc_grids(L->img_h);
  if (sw * sh != L->n || !L->ch[idx][0]) return false;

  auto sample = [&](const uint16_t *src, float x, float y) -> float {
    if (x < 0.f) x = 0.f;
    if (y < 0.f) y = 0.f;
    if (x > (float)(sw - 1)) x = (float)(sw - 1);
    if (y > (float)(sh - 1)) y = (float)(sh - 1);
    const uint32_t x0 = (uint32_t)x;
    const uint32_t y0 = (uint32_t)y;
    uint32_t x1 = x0 + 1;
    uint32_t y1 = y0 + 1;
    if (x1 >= sw) x1 = sw - 1;
    if (y1 >= sh) y1 = sh - 1;
    const float tx = x - (float)x0;
    const float ty = y - (float)y0;
    const float a = lsc_unpack(src[y0 * sw + x0]);
    const float b = lsc_unpack(src[y0 * sw + x1]);
    const float c = lsc_unpack(src[y1 * sw + x0]);
    const float d = lsc_unpack(src[y1 * sw + x1]);
    return a * (1.f - tx) * (1.f - ty) + b * tx * (1.f - ty) + c * (1.f - tx) * ty + d * tx * ty;
  };

  isp_lsc_gain_t *dst[4] = {_lsc_arr.gain_r, _lsc_arr.gain_gr, _lsc_arr.gain_gb, _lsc_arr.gain_b};
  for (int ch = 0; ch < 4; ch++) {
    const uint16_t *src = L->ch[idx][ch];
    for (uint32_t y = 0; y < dh; y++) {
      const float fy = (sh <= 1) ? 0.f : (float)y * (float)(sh - 1) / (float)(dh - 1);
      for (uint32_t x = 0; x < dw; x++) {
        const float fx = (sw <= 1) ? 0.f : (float)x * (float)(sw - 1) / (float)(dw - 1);
        dst[ch][y * dw + x] = lsc_pack(sample(src, fx, fy));
      }
    }
  }

  esp_isp_lsc_config_t cfg = {};
  cfg.gain_array = &_lsc_arr;
  if (esp_isp_lsc_configure(_proc, &cfg) != ESP_OK) {
    Serial.println("CSI: ISP LSC configure failed");
    return false;
  }
  if (!_lsc_on) {
    if (esp_isp_lsc_enable(_proc) != ESP_OK) {
      Serial.println("CSI: ISP LSC enable failed");
      return false;
    }
    _lsc_on = true;
  }
  _lsc_idx = idx;
  return true;
}

bool ESP32P4_Isp::applyBlc(float gain_lin) {
#if __has_include("driver/isp_blc.h")
  if (!_proc || _blc_skip) return true;
#if CONFIG_IDF_TARGET_ESP32P4
  // Hardware BLC exists only on ESP32-P4 v3.0+. v1.x (Guition M3, etc.) returns
  // ESP_ERR_NOT_SUPPORTED and must not be retried every 3A tick.
  if (!_blc_on && ESP.getChipRevision() < 300) {
    _blc_skip = true;
    Serial.printf("CSI: ISP BLC skipped (P4 rev %u, needs v3.0+)\n",
                  (unsigned)ESP.getChipRevision());
    return true;
  }
#endif
  uint16_t off[4] = {16, 16, 16, 16};
  (void)esp32p4_ipa_blc_for_gain(_ipa, gain_lin, off);
  const uint16_t key = (uint16_t)(off[0] ^ (off[1] << 4) ^ (off[2] << 8) ^ (off[3] << 12));
  if (_blc_on && key == _blc_key) return true;

  if (!_blc_on) {
    esp_isp_blc_config_t cfg = {};
    cfg.window.top_left.x = _w / 8;
    cfg.window.top_left.y = _h / 8;
    cfg.window.btm_right.x = _w > 16 ? _w - _w / 8 : _w;
    cfg.window.btm_right.y = _h > 16 ? _h - _h / 8 : _h;
    cfg.filter_enable = false;
    const bool st = _ipa && _ipa->blc_stretch;
    cfg.stretch.top_left_chan_stretch_en = st;
    cfg.stretch.top_right_chan_stretch_en = st;
    cfg.stretch.bottom_left_chan_stretch_en = st;
    cfg.stretch.bottom_right_chan_stretch_en = st;
    if (esp_isp_blc_configure(_proc, &cfg) != ESP_OK) {
      _blc_skip = true;
      Serial.println("CSI: ISP BLC not supported on this chip - skipping");
      return true;
    }
    if (esp_isp_blc_enable(_proc) != ESP_OK) {
      _blc_skip = true;
      Serial.println("CSI: ISP BLC enable failed - skipping");
      return true;
    }
    _blc_on = true;
  }
  esp_isp_blc_offset_t o = {};
  o.top_left_chan_offset = off[0];
  o.top_right_chan_offset = off[1];
  o.bottom_left_chan_offset = off[2];
  o.bottom_right_chan_offset = off[3];
  if (esp_isp_blc_set_correction_offset(_proc, &o) != ESP_OK) {
    _blc_skip = true;
    Serial.println("CSI: ISP BLC offset failed - skipping");
    return true;
  }
  _blc_key = key;
  return true;
#else
  (void)gain_lin;
  return true;
#endif
}

bool ESP32P4_Isp::startAwb() {
  if (_awb) return true;
  const Esp32p4IpaProfile *p = _ipa;
  esp_isp_awb_config_t cfg = {};
  cfg.sample_point = ISP_AWB_SAMPLE_POINT_BEFORE_CCM;
  const uint32_t x0 = _w / 8;
  const uint32_t y0 = _h / 8;
  cfg.window.top_left.x = x0;
  cfg.window.top_left.y = y0;
  cfg.window.btm_right.x = _w - x0;
  cfg.window.btm_right.y = _h - y0;
  cfg.subwindow = cfg.window;
  if (p) {
    cfg.white_patch.luminance.min = p->green_min;
    cfg.white_patch.luminance.max = (uint32_t)p->green_max * 3u;
    cfg.white_patch.red_green_ratio.min = p->rg_min;
    cfg.white_patch.red_green_ratio.max = p->rg_max;
    cfg.white_patch.blue_green_ratio.min = p->bg_min;
    cfg.white_patch.blue_green_ratio.max = p->bg_max;
  } else {
    cfg.white_patch.luminance.min = 16;
    cfg.white_patch.luminance.max = 220 * 3;
    cfg.white_patch.red_green_ratio.min = 0.32f;
    cfg.white_patch.red_green_ratio.max = 0.97f;
    cfg.white_patch.blue_green_ratio.min = 0.22f;
    cfg.white_patch.blue_green_ratio.max = 0.80f;
  }
  if (esp_isp_new_awb_controller(_proc, &cfg, &_awb) != ESP_OK) {
    _awb = nullptr;
    return false;
  }
  esp_isp_awb_cbs_t cbs = {};
  cbs.on_statistics_done = onAwb;
  if (esp_isp_awb_register_event_callbacks(_awb, &cbs, this) != ESP_OK) {
    esp_isp_del_awb_controller(_awb);
    _awb = nullptr;
    return false;
  }
  if (esp_isp_awb_controller_enable(_awb) != ESP_OK) {
    esp_isp_del_awb_controller(_awb);
    _awb = nullptr;
    return false;
  }
  if (esp_isp_awb_controller_start_continuous_statistics(_awb) != ESP_OK) {
    esp_isp_awb_controller_disable(_awb);
    esp_isp_del_awb_controller(_awb);
    _awb = nullptr;
    return false;
  }
  return true;
}

bool ESP32P4_Isp::stopAwb() {
  if (!_awb) return true;
  (void)esp_isp_awb_controller_stop_continuous_statistics(_awb);
  (void)esp_isp_awb_controller_disable(_awb);
  (void)esp_isp_del_awb_controller(_awb);
  _awb = nullptr;
  return true;
}

bool ESP32P4_Isp::startAe() {
  if (_ae) return true;
  esp_isp_ae_config_t cfg = {};
  cfg.sample_point = ISP_AE_SAMPLE_POINT_AFTER_DEMOSAIC;
  const uint32_t x0 = _w / 8;
  const uint32_t y0 = _h / 8;
  cfg.window.top_left.x = x0;
  cfg.window.top_left.y = y0;
  cfg.window.btm_right.x = _w - x0;
  cfg.window.btm_right.y = _h - y0;
  if (esp_isp_new_ae_controller(_proc, &cfg, &_ae) != ESP_OK) {
    _ae = nullptr;
    return false;
  }
  esp_isp_ae_env_detector_evt_cbs_t cbs = {};
  cbs.on_env_statistics_done = onAe;
  if (esp_isp_ae_env_detector_register_event_callbacks(_ae, &cbs, this) != ESP_OK) {
    esp_isp_del_ae_controller(_ae);
    _ae = nullptr;
    return false;
  }
  if (esp_isp_ae_controller_enable(_ae) != ESP_OK) {
    esp_isp_del_ae_controller(_ae);
    _ae = nullptr;
    return false;
  }
  if (esp_isp_ae_controller_start_continuous_statistics(_ae) != ESP_OK) {
    esp_isp_ae_controller_disable(_ae);
    esp_isp_del_ae_controller(_ae);
    _ae = nullptr;
    return false;
  }
  return true;
}

bool ESP32P4_Isp::stopAe() {
  if (!_ae) return true;
  (void)esp_isp_ae_controller_stop_continuous_statistics(_ae);
  (void)esp_isp_ae_controller_disable(_ae);
  (void)esp_isp_del_ae_controller(_ae);
  _ae = nullptr;
  return true;
}

bool ESP32P4_Isp::startHist() {
#if __has_include("driver/isp_hist.h")
  if (_hist) return true;
  if (!_proc || _w < 16 || _h < 16) return false;
  esp_isp_hist_config_t cfg = {};
  cfg.hist_mode = ISP_HIST_SAMPLING_YUV_Y;
  cfg.window.top_left.x = _w / 8;
  cfg.window.top_left.y = _h / 8;
  cfg.window.btm_right.x = _w - _w / 8;
  cfg.window.btm_right.y = _h - _h / 8;
  cfg.rgb_coefficient.coeff_r.decimal = 85;
  cfg.rgb_coefficient.coeff_g.decimal = 85;
  cfg.rgb_coefficient.coeff_b.decimal = 85;
  const uint8_t wdec[25] = {10, 10, 10, 10, 10, 10, 10, 11, 10, 10, 10, 11, 12,
                            11, 10, 10, 10, 11, 10, 10, 10, 10, 10, 10, 10};
  for (int i = 0; i < 25; i++) cfg.window_weight[i].decimal = wdec[i];
  const uint32_t thr[15] = {16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240};
  memcpy(cfg.segment_threshold, thr, sizeof(thr));
  if (esp_isp_new_hist_controller(_proc, &cfg, &_hist) != ESP_OK) {
    cfg.hist_mode = ISP_HIST_SAMPLING_RGB;
    if (esp_isp_new_hist_controller(_proc, &cfg, &_hist) != ESP_OK) {
      _hist = nullptr;
      return false;
    }
  }
  esp_isp_hist_cbs_t cbs = {};
  cbs.on_statistics_done = onHist;
  if (esp_isp_hist_register_event_callbacks(_hist, &cbs, this) != ESP_OK) {
    esp_isp_del_hist_controller(_hist);
    _hist = nullptr;
    return false;
  }
  if (esp_isp_hist_controller_enable(_hist) != ESP_OK) {
    esp_isp_del_hist_controller(_hist);
    _hist = nullptr;
    return false;
  }
  if (esp_isp_hist_controller_start_continuous_statistics(_hist) != ESP_OK) {
    esp_isp_hist_controller_disable(_hist);
    esp_isp_del_hist_controller(_hist);
    _hist = nullptr;
    return false;
  }
  return true;
#else
  return false;
#endif
}

bool ESP32P4_Isp::stopHist() {
#if __has_include("driver/isp_hist.h")
  if (!_hist) return true;
  (void)esp_isp_hist_controller_stop_continuous_statistics(_hist);
  (void)esp_isp_hist_controller_disable(_hist);
  (void)esp_isp_del_hist_controller(_hist);
  _hist = nullptr;
#endif
  return true;
}

bool ESP32P4_Isp::setAwb(bool on) {
  _awb_on = on;
  if (!_proc) return true;
  if (on) return startAwb();
  return stopAwb();
}

bool ESP32P4_Isp::setAe(bool on) {
  _ae_on = on;
  _manual_once = false;
  if (!_proc) return true;
  if (on) return startAe();
  return stopAe();
}

void ESP32P4_Isp::setAeTarget(uint8_t luma) {
  if (luma < 20) luma = 20;
  if (luma > 180) luma = 180;
  _ae_target = luma;
  _ae_user = true;
}

void ESP32P4_Isp::setEvBias(int half_stops) {
  if (half_stops < -4) half_stops = -4;
  if (half_stops > 4) half_stops = 4;
  _ev_bias = half_stops;
}

void ESP32P4_Isp::setAntiFlicker(uint8_t hz) {
  _ac_hz = (hz == 50 || hz == 60) ? hz : 0;
}

void ESP32P4_Isp::setAeLimits(uint16_t exp_min, uint16_t exp_max, uint16_t gain_min,
                              uint16_t gain_max) {
  if (exp_min < 4) exp_min = 4;
  if (exp_max < exp_min) exp_max = exp_min;
  if (gain_min < 1) gain_min = 1;
  if (gain_max < gain_min) gain_max = gain_min;
  _exp_min = exp_min;
  _exp_max = exp_max;
  _gain_min = gain_min;
  _gain_max = gain_max;
}

bool ESP32P4_Isp::setBrightness(int8_t v) {
  _brightness = v;
  return applyColor();
}

bool ESP32P4_Isp::setContrast(uint8_t v) {
  _contrast = v;
  return applyColor();
}

bool ESP32P4_Isp::setSaturation(uint8_t v) {
  _saturation = v;
  return applyColor();
}

bool ESP32P4_Isp::setHue(uint16_t deg) {
  _hue = (uint16_t)(deg % 360);
  return applyColor();
}

static int32_t clamp_wb_1024(int32_t v) {
  if (v < 256) v = 256;
  if (v > 4096) v = 4096;
  return v;
}

bool ESP32P4_Isp::setRedBalance(int32_t v1024) {
  _rb_1024 = clamp_wb_1024(v1024);
  _user_rgain = (float)_rb_1024 / 1024.f;
  (void)setAwb(false);
  return applyCcm();
}

bool ESP32P4_Isp::setBlueBalance(int32_t v1024) {
  _bb_1024 = clamp_wb_1024(v1024);
  _user_bgain = (float)_bb_1024 / 1024.f;
  (void)setAwb(false);
  return applyCcm();
}

bool ESP32P4_Isp::setSharpness(uint8_t v) {
  _sharpness = v;
  return applySharpen();
}

bool ESP32P4_Isp::setDenoise(uint8_t v) {
  _denoise = v > 8 ? 8 : v;
  return applyBf();
}

bool ESP32P4_Isp::applyBf() {
  if (!_proc) return false;
  const Esp32p4IpaProfile *p = _ipa;
  esp_isp_bf_config_t bf = {};
  bf.padding_mode = ISP_BF_EDGE_PADDING_MODE_SRND_DATA;
  bf.denoising_level = _denoise;
  if (_bf_user) {
    memcpy(bf.bf_template, _bf_mat, 9);
  } else if (p && p->bf_mat) {
    memcpy(bf.bf_template, p->bf_mat, 9);
  } else {
    const uint8_t kBf[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
    memcpy(bf.bf_template, kBf, 9);
  }
  if (esp_isp_bf_configure(_proc, &bf) != ESP_OK) return false;
  return esp_isp_bf_enable(_proc) == ESP_OK;
}

bool ESP32P4_Isp::applySharpen() {
  if (!_proc) return false;
  if (_sharpness == 0) {
    (void)esp_isp_sharpen_disable(_proc);
    return true;
  }
  const Esp32p4IpaProfile *p = _ipa;
  const float scale = (float)_sharpness / 128.f;
  float hc = (_sh_user ? _sh_hc : (p ? p->sh_hc : 2.0f)) * scale;
  float mc = (_sh_user ? _sh_mc : (p ? p->sh_mc : 1.5f)) * scale;
  if (hc > 7.9f) hc = 7.9f;
  if (mc > 7.9f) mc = 7.9f;
  esp_isp_sharpen_config_t sh = {};
  sh.h_thresh = _sh_user ? _sh_h : (p ? p->sh_h : 20);
  sh.l_thresh = _sh_user ? _sh_l : (p ? p->sh_l : 4);
  uint32_t hi = 2, hd = 16, mi = 1, md = 16;
  f_to_fixed(hc, ISP_SHARPEN_H_FREQ_COEF_INT_BITS, ISP_SHARPEN_H_FREQ_COEF_DEC_BITS, &hi, &hd);
  f_to_fixed(mc, ISP_SHARPEN_M_FREQ_COEF_INT_BITS, ISP_SHARPEN_M_FREQ_COEF_DEC_BITS, &mi, &md);
  sh.h_freq_coeff.integer = hi;
  sh.h_freq_coeff.decimal = hd;
  sh.m_freq_coeff.integer = mi;
  sh.m_freq_coeff.decimal = md;
  sh.padding_mode = ISP_SHARPEN_EDGE_PADDING_MODE_SRND_DATA;
  if (_sh_user) {
    memcpy(sh.sharpen_template, _sh_mat, 9);
  } else if (p && p->sh_mat) {
    memcpy(sh.sharpen_template, p->sh_mat, 9);
  } else {
    const uint8_t kSh[9] = {1, 2, 1, 2, 2, 2, 1, 2, 1};
    memcpy(sh.sharpen_template, kSh, 9);
  }
  if (esp_isp_sharpen_configure(_proc, &sh) != ESP_OK) return false;
  return esp_isp_sharpen_enable(_proc) == ESP_OK;
}

void ESP32P4_Isp::process(ESP32P4_Camera *cam) {
  if (!_proc || !cam) return;
  const uint32_t now = millis();
  const Esp32p4IpaProfile *p = _ipa;

  if (_awb_on && _awb_fresh && (now - _last_awb_ms) >= ESP32P4_ISP_3A_MS) {
    _awb_fresh = 0;
    _last_awb_ms = now;
    const uint32_t n = _white_n;
    const uint32_t sr = _sum_r;
    const uint32_t sg = _sum_g;
    const uint32_t sb = _sum_b;
    const uint32_t need = p && p->min_counted ? p->min_counted : 80;
    float rg = 0.f, bg = 0.f;
    bool ok = false;
#if ISP_AWB_WINDOW_X_NUM > 0 && ISP_AWB_WINDOW_Y_NUM > 0
    {
      float srg = 0.f, sbg = 0.f, sw = 0.f;
      for (int y = 0; y < ISP_AWB_WINDOW_Y_NUM; y++) {
        for (int x = 0; x < ISP_AWB_WINDOW_X_NUM; x++) {
          const uint32_t cn = _sw_n[x][y];
          const uint32_t cg = _sw_g[x][y];
          if (cn < 8 || cg == 0) continue;
          const float cr = (float)_sw_r[x][y] / (float)cn;
          const float cgg = (float)cg / (float)cn;
          const float cb = (float)_sw_b[x][y] / (float)cn;
          if (cgg < 1.f) continue;
          float crg = cr / cgg;
          float cbg = cb / cgg;
          if (esp32p4_ipa_awb_zone(p, crg, cbg) >= 5) continue;
          srg += crg * (float)cn;
          sbg += cbg * (float)cn;
          sw += (float)cn;
        }
      }
      if (sw >= (float)need * 0.25f && srg > 0.f && sbg > 0.f) {
        rg = srg / sw;
        bg = sbg / sw;
        ok = true;
      }
    }
#endif
    if (!ok && n >= need && sg > 0 && sr > 0 && sb > 0) {
      const float r = (float)sr / (float)n;
      const float g = (float)sg / (float)n;
      const float b = (float)sb / (float)n;
      rg = r / g;
      bg = b / g;
      if (esp32p4_ipa_awb_zone(p, rg, bg) < 5) ok = true;
    }
    if (ok && rg > 0.02f && bg > 0.02f) {
      esp32p4_ipa_awb_attract(p, &rg, &bg);
      const float nw = p ? p->new_w : 0.18f;
      const float pw = p ? p->prev_w : 0.82f;
      const float rs = p ? p->r_scale : 1.f;
      const float bs = p ? p->b_scale : 1.f;
      float rgn = (1.f / rg) * rs;
      float bgn = (1.f / bg) * bs;
      if (rgn < 0.5f) rgn = 0.5f;
      if (rgn > 3.2f) rgn = 3.2f;
      if (bgn < 0.5f) bgn = 0.5f;
      if (bgn > 3.2f) bgn = 3.2f;
      _rgain = _rgain * pw + rgn * nw;
      _bgain = _bgain * pw + bgn * nw;
      _ct = _ct * pw + esp32p4_ipa_ct_from_rgbg(p, rg, bg) * nw;
      (void)applyCcm();
      (void)applyLsc(_ct);
    }
  }

  if (_ae_on && _ae_fresh && (now - _last_ae_ms) >= ESP32P4_ISP_3A_MS) {
    _ae_fresh = 0;
    _last_ae_ms = now;
    unsigned sum = 0, wt = 0, hot = 0, cold = 0;
    const uint8_t hi_thr = p ? p->ae_hi_thr : 248;
    const uint8_t hi_reg = p ? p->ae_hi_reg : 4;
    const uint8_t lo_thr = p ? p->ae_lo_thr : 13;
    const uint8_t lo_reg = p ? p->ae_lo_reg : 5;
    const uint8_t *wtab = p && p->ae_wt ? p->ae_wt : nullptr;
    for (int y = 0; y < ISP_AE_BLOCK_Y_NUM && y < 5; y++) {
      for (int x = 0; x < ISP_AE_BLOCK_X_NUM && x < 5; x++) {
        unsigned v = _ae_luma[x][y];
        unsigned w = wtab ? wtab[y * 5 + x] : 1u;
        if (!w) w = 1;
        sum += v * w;
        wt += w;
        if (v >= hi_thr) hot++;
        if (v <= lo_thr) cold++;
      }
    }
    if (!wt) return;
    float luma = (float)sum / (float)wt;
    _last_luma = luma;
    if (!_manual_once) {
      uint16_t e = 0, g = 0;
      if (cam->getExposure(&e) && e >= _exp_min) _exp = e;
      if (cam->getGain(&g) && g >= _gain_min) _gain = g;
      (void)cam->setAEC(false);
      (void)cam->setAGC(false);
      _manual_once = true;
    }

    const uint32_t line_us = cam->lineTimeUs() ? cam->lineTimeUs() : 30u;
    const float exp_us = (float)_exp * (float)line_us;
    float gain_lin = (float)_gain / 16.f;
    if (gain_lin < 0.25f) gain_lin = 0.25f;
    float env = 0.f;
#if __has_include("driver/isp_hist.h")
    if (p && p->env_k > 0.f && p->env_sp && p->env_sp_n >= 16 && _hist_fresh) {
      _hist_fresh = 0;
      float conv = 0.f;
      uint32_t hn = 0;
      for (int i = 0; i < 16; i++) {
        conv += p->env_sp[i] * (float)_hist_bin[i];
        hn += _hist_bin[i];
      }
      const float denom = exp_us * gain_lin;
      if (hn && denom > 1.f) env = p->env_k * fabsf(conv) / denom;
    } else
#endif
        if (p && p->env_k > 0.f) {
      const float denom = exp_us * gain_lin;
      if (denom > 1.f) env = p->env_k * luma / denom;
    }
    _env_luma = env;

    float tgt = (float)_ae_target;
    if (p && !_ae_user) {
      if (luma < (float)p->ae_low) tgt = (float)p->ae_low;
      else if (luma > (float)p->ae_high) tgt = (float)p->ae_high;
      else tgt = (float)p->ae_target;
    }
    tgt += esp32p4_ipa_pwl_shift(p, env);
    if (_ev_bias) tgt *= powf(1.41421356f, (float)_ev_bias * 0.5f);
    if (tgt < 20.f) tgt = 20.f;
    if (tgt > 180.f) tgt = 180.f;

    float err = tgt - luma;
    if ((int)hot >= (int)hi_reg && luma > tgt) err = (tgt * 0.85f) - luma;
    if ((int)cold >= (int)lo_reg && luma < tgt) err = (tgt * 1.10f) - luma;
    if (err > -4.0f && err < 4.0f) return;

    float ratio = tgt / (luma < 8.0f ? 8.0f : luma);
    const float inc = p && p->inc_r > 0.05f ? p->inc_r : 0.32f;
    const float dec = p && p->dec_r > 0.05f ? p->dec_r : 0.42f;
    if (ratio > 1.f) ratio = 1.f + (ratio - 1.f) * inc;
    else ratio = 1.f - (1.f - ratio) * dec;
    if (ratio > 1.8f) ratio = 1.8f;
    if (ratio < 0.55f) ratio = 0.55f;

    if (ratio > 1.0f) {
      uint32_t ne = (uint32_t)((float)_exp * ratio + 0.5f);
      if (ne > _exp_max) {
        float leftover = (float)ne / (float)_exp_max;
        _exp = _exp_max;
        uint32_t ng = (uint32_t)((float)_gain * leftover + 0.5f);
        if (ng > _gain_max) ng = _gain_max;
        _gain = (uint16_t)ng;
      } else {
        if (ne < _exp_min) ne = _exp_min;
        _exp = (uint16_t)ne;
      }
    } else {
      uint32_t ng = (uint32_t)((float)_gain * ratio + 0.5f);
      if (ng < _gain_min) {
        float leftover = (float)ng / (float)_gain_min;
        _gain = _gain_min;
        uint32_t ne = (uint32_t)((float)_exp * leftover + 0.5f);
        if (ne < _exp_min) ne = _exp_min;
        _exp = (uint16_t)ne;
      } else {
        _gain = (uint16_t)ng;
      }
    }

    const uint8_t hz = _ac_hz ? _ac_hz : (p ? p->ac_hz : 0);
    if (hz == 50 || hz == 60) {
      const uint32_t flicker_us = 1000000u / (2u * (uint32_t)hz);
      uint32_t fl = flicker_us / (line_us < 8 ? 8 : line_us);
      if (fl < 8) fl = 8;
      if (_exp > fl) {
        uint32_t n = (_exp + fl / 2) / fl;
        if (n < 1) n = 1;
        uint32_t snapped = n * fl;
        if (snapped < _exp_min) snapped = _exp_min;
        if (snapped > _exp_max) snapped = _exp_max;
        _exp = (uint16_t)snapped;
      }
    }
    (void)cam->setExposure(_exp);
    (void)cam->setGain(_gain);
    (void)applyBlc((float)_gain / 16.f);
  }
}

static void isp_clamp_win(isp_window_t *w, uint32_t W, uint32_t H) {
  if (!w || W < 32 || H < 32) return;
  if (w->top_left.x >= W) w->top_left.x = 0;
  if (w->top_left.y >= H) w->top_left.y = 0;
  if (w->btm_right.x > W) w->btm_right.x = W;
  if (w->btm_right.y > H) w->btm_right.y = H;
  w->top_left.x &= ~1u;
  w->top_left.y &= ~1u;
  w->btm_right.x &= ~1u;
  w->btm_right.y &= ~1u;
  if (w->btm_right.x < w->top_left.x + 16) {
    w->btm_right.x = w->top_left.x + 32;
    if (w->btm_right.x > W) {
      w->btm_right.x = W & ~1u;
      if (w->btm_right.x > 32) w->top_left.x = (w->btm_right.x - 32) & ~1u;
    }
  }
  if (w->btm_right.y < w->top_left.y + 16) {
    w->btm_right.y = w->top_left.y + 32;
    if (w->btm_right.y > H) {
      w->btm_right.y = H & ~1u;
      if (w->btm_right.y > 32) w->top_left.y = (w->btm_right.y - 32) & ~1u;
    }
  }
}

uint16_t ESP32P4_Isp::afMaxPos() const {
  return (_sensor == ESP32P4_SENSOR_OV5647) ? 500 : 1023;
}

bool ESP32P4_Isp::startAf() {
#if __has_include("driver/isp_af.h") && ISP_AF_WINDOW_NUM > 0
  if (_af) return true;
  if (!_proc || _w < 32 || _h < 32) return false;
  uint32_t ww, hh, l, t;
  if (_sensor == ESP32P4_SENSOR_OV5647) {
    l = 680u * _w / 1920u;
    t = 300u * _h / 1080u;
    ww = 390u * _w / 1920u;
    hh = 410u * _h / 1080u;
  } else {
    ww = _w / 3;
    hh = _h / 3;
    l = (_w - ww) / 2;
    t = (_h - hh) / 2;
  }
  if (ww < 32) ww = 32;
  if (hh < 32) hh = 32;
  esp_isp_af_config_t cfg = {};
  cfg.edge_thresh = (_sensor == ESP32P4_SENSOR_OV5647) ? 32 : 128;
  cfg.window[0].top_left.x = l;
  cfg.window[0].top_left.y = t;
  cfg.window[0].btm_right.x = l + ww;
  cfg.window[0].btm_right.y = t + hh;
  const uint32_t shift = ww / 2;
  cfg.window[1].top_left.x = (l > shift) ? l - shift : 0;
  cfg.window[1].top_left.y = t;
  cfg.window[1].btm_right.x = cfg.window[1].top_left.x + ww;
  cfg.window[1].btm_right.y = t + hh;
  cfg.window[2].top_left.x = l + shift;
  cfg.window[2].top_left.y = t;
  cfg.window[2].btm_right.x = cfg.window[2].top_left.x + ww;
  cfg.window[2].btm_right.y = t + hh;
  for (int i = 0; i < ISP_AF_WINDOW_NUM && i < 3; i++) isp_clamp_win(&cfg.window[i], _w, _h);
  if (esp_isp_new_af_controller(_proc, &cfg, &_af) != ESP_OK) {
    _af = nullptr;
    return false;
  }
  if (esp_isp_af_controller_enable(_af) != ESP_OK) {
    (void)esp_isp_del_af_controller(_af);
    _af = nullptr;
    return false;
  }
  return true;
#else
  return false;
#endif
}

bool ESP32P4_Isp::stopAf() {
#if __has_include("driver/isp_af.h")
  if (!_af) return true;
  (void)esp_isp_af_controller_disable(_af);
  (void)esp_isp_del_af_controller(_af);
  _af = nullptr;
#endif
  return true;
}

uint32_t ESP32P4_Isp::afScore() {
#if __has_include("driver/isp_af.h") && ISP_AF_WINDOW_NUM > 0
  if (!_af) return 0;
  isp_af_result_t r{};
  if (esp_isp_af_controller_get_oneshot_statistics(_af, 80, &r) != ESP_OK) return 0;
  static const uint8_t kWt[3] = {4, 1, 1};
  uint32_t s = 0;
  for (int i = 0; i < ISP_AF_WINDOW_NUM && i < 3; i++) {
    if (r.luminance[i] < 12) continue;
    int d = r.definition[i];
    if (d < 0) d = 0;
    s += (uint32_t)d * kWt[i];
  }
  return s;
#else
  return 0;
#endif
}

#if __has_include("esp_video_isp_ioctl.h")
#include "esp_video_isp_ioctl.h"

bool ESP32P4_Isp::exportV4l2Stats(esp_video_isp_stats_t *out) const {
  if (!out || !_proc) return false;
  memset(out, 0, sizeof(*out));
  static uint32_t s_seq = 0;
  out->seq = ++s_seq;
  out->flags = ESP_VIDEO_ISP_STATS_FLAG_AE | ESP_VIDEO_ISP_STATS_FLAG_AWB;
  if (_ae_fresh) {
    for (int y = 0; y < ISP_AE_BLOCK_Y_NUM; y++) {
      for (int x = 0; x < ISP_AE_BLOCK_X_NUM; x++) {
        out->ae.ae_result.luminance[x][y] = _ae_luma[x][y];
      }
    }
  }
  out->awb.awb_result.white_patch_num = _white_n;
  out->awb.awb_result.sum_r = _sum_r;
  out->awb.awb_result.sum_g = _sum_g;
  out->awb.awb_result.sum_b = _sum_b;
  if (_af) out->flags |= ESP_VIDEO_ISP_STATS_FLAG_AF;
#if __has_include("driver/isp_hist.h")
  if (_hist) out->flags |= ESP_VIDEO_ISP_STATS_FLAG_HIST;
#endif
  return true;
}

bool ESP32P4_Isp::importV4l2Cid(uint32_t id, const void *data, size_t size) {
  if (!_proc || !data) return false;
  switch (id) {
    case V4L2_CID_USER_ESP_ISP_CCM: {
      if (size < sizeof(esp_video_isp_ccm_t)) return false;
      auto *c = (const esp_video_isp_ccm_t *)data;
      if (!c->enable) {
        _ccm_user = false;
        return applyCcm();
      }
      _ccm_user = true;
      memcpy(_ccm_m, c->matrix, sizeof(_ccm_m));
      return applyCcm() && esp_isp_ccm_enable(_proc) == ESP_OK;
    }
    case V4L2_CID_USER_ESP_ISP_GAMMA: {
      if (size < sizeof(esp_video_isp_gamma_t)) return false;
      auto *g = (const esp_video_isp_gamma_t *)data;
      if (!g->enable) {
        (void)esp_isp_gamma_disable(_proc);
        return true;
      }
      isp_gamma_curve_points_t pts = {};
      for (int i = 0; i < ISP_GAMMA_CURVE_POINTS_NUM; i++) {
        pts.pt[i].x = g->points[i].x;
        pts.pt[i].y = g->points[i].y;
      }
      if (esp_isp_gamma_configure(_proc, COLOR_COMPONENT_R, &pts) != ESP_OK) return false;
      if (esp_isp_gamma_configure(_proc, COLOR_COMPONENT_G, &pts) != ESP_OK) return false;
      if (esp_isp_gamma_configure(_proc, COLOR_COMPONENT_B, &pts) != ESP_OK) return false;
      return esp_isp_gamma_enable(_proc) == ESP_OK;
    }
    case V4L2_CID_USER_ESP_ISP_BF: {
      if (size < sizeof(esp_video_isp_bf_t)) return false;
      auto *b = (const esp_video_isp_bf_t *)data;
      if (!b->enable) {
        (void)esp_isp_bf_disable(_proc);
        return true;
      }
      _bf_user = true;
      _denoise = b->level > 8 ? 8 : b->level;
      memcpy(_bf_mat, b->matrix, 9);
      return applyBf();
    }
    case V4L2_CID_USER_ESP_ISP_SHARPEN: {
      if (size < sizeof(esp_video_isp_sharpen_t)) return false;
      auto *s = (const esp_video_isp_sharpen_t *)data;
      if (!s->enable) {
        _sharpness = 0;
        return applySharpen();
      }
      _sh_user = true;
      _sh_h = s->h_thresh;
      _sh_l = s->l_thresh;
      _sh_hc = s->h_coeff;
      _sh_mc = s->m_coeff;
      memcpy(_sh_mat, s->matrix, 9);
      if (_sharpness == 0) _sharpness = 128;
      return applySharpen();
    }
    case V4L2_CID_USER_ESP_ISP_DEMOSAIC: {
      if (size < sizeof(esp_video_isp_demosaic_t)) return false;
      auto *d = (const esp_video_isp_demosaic_t *)data;
      _dm_grad = d->gradient_ratio > 0.01f ? d->gradient_ratio : 1.5f;
      esp_isp_demosaic_config_t dm = {};
      dm.padding_mode = ISP_DEMOSAIC_EDGE_PADDING_MODE_SRND_DATA;
      uint32_t gi = 1, gd = 8;
      f_to_fixed(_dm_grad, ISP_DEMOSAIC_GRAD_RATIO_INT_BITS, ISP_DEMOSAIC_GRAD_RATIO_DEC_BITS, &gi,
                 &gd);
      dm.grad_ratio.integer = gi;
      dm.grad_ratio.decimal = gd;
      if (esp_isp_demosaic_configure(_proc, &dm) != ESP_OK) return false;
      return d->enable ? (esp_isp_demosaic_enable(_proc) == ESP_OK)
                       : (esp_isp_demosaic_disable(_proc) == ESP_OK);
    }
    case V4L2_CID_USER_ESP_ISP_WB: {
      if (size < sizeof(esp_video_isp_wb_t)) return false;
      auto *w = (const esp_video_isp_wb_t *)data;
      if (!w->enable) return setAwb(true);
      (void)setAwb(false);
      return setRedBalance((int32_t)(w->red_gain * 1024.f)) &&
             setBlueBalance((int32_t)(w->blue_gain * 1024.f));
    }
    case V4L2_CID_USER_ESP_ISP_LSC: {
      if (size < sizeof(esp_video_isp_lsc_t)) return false;
      auto *l = (const esp_video_isp_lsc_t *)data;
      if (!l->enable) {
        if (_lsc_on) {
          (void)esp_isp_lsc_disable(_proc);
          _lsc_on = false;
        }
        return true;
      }
      if (!l->gain_r || !l->lsc_gain_size) return applyLsc(_ct);
      if (!_lsc_arr.gain_r) {
        if (esp_isp_lsc_allocate_gain_array(_proc, &_lsc_arr, &_lsc_n) != ESP_OK) return false;
      }
      if (l->lsc_gain_size != _lsc_n) return false;
      memcpy(_lsc_arr.gain_r, l->gain_r, _lsc_n * sizeof(isp_lsc_gain_t));
      if (l->gain_gr) memcpy(_lsc_arr.gain_gr, l->gain_gr, _lsc_n * sizeof(isp_lsc_gain_t));
      if (l->gain_gb) memcpy(_lsc_arr.gain_gb, l->gain_gb, _lsc_n * sizeof(isp_lsc_gain_t));
      if (l->gain_b) memcpy(_lsc_arr.gain_b, l->gain_b, _lsc_n * sizeof(isp_lsc_gain_t));
      esp_isp_lsc_config_t lcfg = {};
      lcfg.gain_array = &_lsc_arr;
      if (esp_isp_lsc_configure(_proc, &lcfg) != ESP_OK) return false;
      if (!_lsc_on && esp_isp_lsc_enable(_proc) == ESP_OK) _lsc_on = true;
      return _lsc_on;
    }
    case V4L2_CID_USER_ESP_ISP_AF: {
      if (size < sizeof(esp_video_isp_af_t)) return false;
      auto *a = (const esp_video_isp_af_t *)data;
      if (!a->enable) return stopAf();
      (void)stopAf();
      return startAf();
    }
    case V4L2_CID_USER_ESP_ISP_AWB: {
      if (size < sizeof(esp_video_isp_awb_t)) return false;
      auto *a = (const esp_video_isp_awb_t *)data;
      if (!a->enable) return setAwb(false);
      return setAwb(true);
    }
    case V4L2_CID_USER_ESP_ISP_BLC: {
      if (size < sizeof(esp_video_isp_blc_t)) return false;
      auto *b = (const esp_video_isp_blc_t *)data;
#if __has_include("driver/isp_blc.h")
      _blc_off[0] = b->top_left_offset;
      _blc_off[1] = b->top_right_offset;
      _blc_off[2] = b->bottom_left_offset;
      _blc_off[3] = b->bottom_right_offset;
      _blc_stretch = b->stretch_enable;
      _blc_key = 0xffff;
      if (!b->enable) {
        if (_blc_on) {
          (void)esp_isp_blc_disable(_proc);
          _blc_on = false;
        }
        return true;
      }
      return applyBlc(_gain > 16 ? (float)_gain / 16.f : 1.f);
#else
      (void)b;
      return false;
#endif
    }
    default:
      return false;
  }
}

bool ESP32P4_Isp::exportV4l2Cid(uint32_t id, void *data, size_t size) const {
  if (!data) return false;
  switch (id) {
    case V4L2_CID_USER_ESP_ISP_CCM: {
      if (size < sizeof(esp_video_isp_ccm_t)) return false;
      auto *c = (esp_video_isp_ccm_t *)data;
      memset(c, 0, sizeof(*c));
      c->enable = true;
      memcpy(c->matrix, _ccm_m, sizeof(_ccm_m));
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_GAMMA: {
      if (size < sizeof(esp_video_isp_gamma_t)) return false;
      auto *g = (esp_video_isp_gamma_t *)data;
      memset(g, 0, sizeof(*g));
      g->enable = true;
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_BF: {
      if (size < sizeof(esp_video_isp_bf_t)) return false;
      auto *b = (esp_video_isp_bf_t *)data;
      memset(b, 0, sizeof(*b));
      b->enable = true;
      b->level = _denoise;
      memcpy(b->matrix, _bf_mat, 9);
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_SHARPEN: {
      if (size < sizeof(esp_video_isp_sharpen_t)) return false;
      auto *s = (esp_video_isp_sharpen_t *)data;
      memset(s, 0, sizeof(*s));
      s->enable = _sharpness > 0;
      s->h_thresh = _sh_h;
      s->l_thresh = _sh_l;
      s->h_coeff = _sh_hc;
      s->m_coeff = _sh_mc;
      memcpy(s->matrix, _sh_mat, 9);
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_DEMOSAIC: {
      if (size < sizeof(esp_video_isp_demosaic_t)) return false;
      auto *d = (esp_video_isp_demosaic_t *)data;
      memset(d, 0, sizeof(*d));
      d->enable = true;
      d->gradient_ratio = _dm_grad;
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_WB: {
      if (size < sizeof(esp_video_isp_wb_t)) return false;
      auto *w = (esp_video_isp_wb_t *)data;
      memset(w, 0, sizeof(*w));
      w->enable = !_awb_on;
      w->red_gain = (float)_rb_1024 / 1024.f;
      w->blue_gain = (float)_bb_1024 / 1024.f;
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_LSC: {
      if (size < sizeof(esp_video_isp_lsc_t)) return false;
      auto *l = (esp_video_isp_lsc_t *)data;
      memset(l, 0, sizeof(*l));
      l->enable = _lsc_on;
      l->gain_r = _lsc_arr.gain_r;
      l->gain_gr = _lsc_arr.gain_gr;
      l->gain_gb = _lsc_arr.gain_gb;
      l->gain_b = _lsc_arr.gain_b;
      l->lsc_gain_size = _lsc_n;
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_AF: {
      if (size < sizeof(esp_video_isp_af_t)) return false;
      auto *a = (esp_video_isp_af_t *)data;
      memset(a, 0, sizeof(*a));
      a->enable = _af != nullptr;
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_AWB: {
      if (size < sizeof(esp_video_isp_awb_t)) return false;
      auto *a = (esp_video_isp_awb_t *)data;
      memset(a, 0, sizeof(*a));
      a->enable = _awb_on;
      return true;
    }
    case V4L2_CID_USER_ESP_ISP_BLC: {
      if (size < sizeof(esp_video_isp_blc_t)) return false;
      auto *b = (esp_video_isp_blc_t *)data;
      memset(b, 0, sizeof(*b));
      b->enable = _blc_on;
      b->stretch_enable = _blc_stretch;
      b->top_left_offset = _blc_off[0];
      b->top_right_offset = _blc_off[1];
      b->bottom_left_offset = _blc_off[2];
      b->bottom_right_offset = _blc_off[3];
      return true;
    }
    default:
      return false;
  }
}
#endif
