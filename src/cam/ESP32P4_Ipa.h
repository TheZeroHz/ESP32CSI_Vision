#pragma once

/**
 * In-process IPA tables (no V4L2). Data is Espressif sensor JSON
 * (ESPRESSIF MIT) applied through the P4 ISP driver.
 */

#include <stdint.h>

#include "cam/ESP32P4_Camera.h"

struct Esp32p4IpaCcmRow {
  float ct;
  float m[9];
};

struct Esp32p4IpaAwbRef {
  float ct;
  float rg;
  float bg;
  float radius;
};

struct Esp32p4IpaAwbZone {
  float type;
  float rg_min, rg_max, bg_min, bg_max;
  float enabled;
  float pad;
};

struct Esp32p4IpaPwl {
  float env;
  float shift;
};

struct Esp32p4IpaBlc {
  float gain;
  float tl, tr, bl, br;
};

struct Esp32p4IpaLsc {
  uint16_t img_w;
  uint16_t img_h;
  uint16_t n;
  uint8_t sets;
  const uint16_t *ct;
  const uint16_t *const (*ch)[4];
};

struct Esp32p4IpaProfile {
  const char *name;
  float rg_min, rg_max, bg_min, bg_max;
  uint8_t green_min, green_max;
  float new_w, prev_w, r_scale, b_scale;
  uint32_t min_counted;
  const Esp32p4IpaAwbRef *awb_ref;
  uint8_t awb_ref_n;
  const Esp32p4IpaCcmRow *ccm;
  uint8_t ccm_n;
  float low_luma_thr;
  const float *low_luma_m;
  uint8_t ae_target, ae_low, ae_high, ae_hi_thr, ae_hi_reg;
  uint8_t ac_hz;
  float gamma;
  uint8_t sat, contrast;
  uint8_t bf_level;
  const uint8_t *bf_mat;
  uint8_t sh_h, sh_l;
  float sh_hc, sh_mc;
  const uint8_t *sh_mat;
  const Esp32p4IpaLsc *lsc;
  uint8_t ae_lo_thr, ae_lo_reg;
  const uint8_t *ae_wt;
  float inc_r, dec_r;
  float env_k;
  const float *env_sp;
  uint8_t env_sp_n;
  const Esp32p4IpaPwl *pwl;
  uint8_t pwl_n;
  const Esp32p4IpaBlc *blc;
  uint8_t blc_n;
  uint8_t blc_stretch;
  const Esp32p4IpaAwbZone *awb_zone;
  uint8_t awb_zone_n;
};

const Esp32p4IpaProfile *esp32p4_ipa_profile(esp32p4_cam_sensor_t id);
void esp32p4_ipa_interp_ccm(const Esp32p4IpaProfile *p, float ct, float luma, float out[9]);
float esp32p4_ipa_ct_from_rgbg(const Esp32p4IpaProfile *p, float rg, float bg);
uint16_t esp32p4_ipa_nearest_lsc(const Esp32p4IpaLsc *lsc, float ct);
/** Attract (rg,bg) toward the nearest calibrated locus if inside radius. */
void esp32p4_ipa_awb_attract(const Esp32p4IpaProfile *p, float *rg, float *bg);
/** Zone type 0–4 illuminant, 5 green, 6 skin; -1 if none. */
int esp32p4_ipa_awb_zone(const Esp32p4IpaProfile *p, float rg, float bg);
float esp32p4_ipa_pwl_shift(const Esp32p4IpaProfile *p, float env_luma);
bool esp32p4_ipa_blc_for_gain(const Esp32p4IpaProfile *p, float gain, uint16_t out[4]);
