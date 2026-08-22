#include "cam/ESP32P4_Ipa.h"

#include <math.h>

#include "cam/ipa/esp32p4_ipa_gc2607.h"
#include "cam/ipa/esp32p4_ipa_mira220.h"
#include "cam/ipa/esp32p4_ipa_os02n10.h"
#include "cam/ipa/esp32p4_ipa_os04c10.h"
#include "cam/ipa/esp32p4_ipa_ov2710.h"
#include "cam/ipa/esp32p4_ipa_ov5647.h"
#include "cam/ipa/esp32p4_ipa_ov9281.h"
#include "cam/ipa/esp32p4_ipa_sc035hgs.h"
#include "cam/ipa/esp32p4_ipa_sc1346.h"
#include "cam/ipa/esp32p4_ipa_sc202cs.h"
#include "cam/ipa/esp32p4_ipa_sc2331.h"
#include "cam/ipa/esp32p4_ipa_sc2336.h"
#include "cam/ipa/esp32p4_ipa_sti2250.h"

static const Esp32p4IpaAwbRef kGenericAwb[] = {
    {2800.f, 0.90f, 0.35f, 0.f}, {4000.f, 0.72f, 0.48f, 0.f}, {5500.f, 0.58f, 0.58f, 0.f},
    {6500.f, 0.51f, 0.63f, 0.f}, {9000.f, 0.40f, 0.70f, 0.f},
};

static const Esp32p4IpaCcmRow kGenericCcm[] = {
    {5000.f, {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f}},
};

static const float kIdent9[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
static const uint8_t kBfGeneric[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
static const uint8_t kShGeneric[9] = {1, 2, 1, 2, 2, 2, 1, 2, 1};

static const uint8_t kAeWtGeneric[25] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static const Esp32p4IpaBlc kBlcGeneric[] = {{1.f, 16.f, 16.f, 16.f, 16.f}};

#define IPA_LSC(pfx)                                                                 \
  static const Esp32p4IpaLsc s_lsc_##pfx = {k##pfx##LscW, k##pfx##LscH, k##pfx##LscN, \
                                            k##pfx##LscSets, k##pfx##LscCt, k##pfx##LscCh};

#define IPA_FILL(disp, pfx)                                                            \
  {                                                                                    \
      disp,                                                                            \
      k##pfx##RgMin,                                                                   \
      k##pfx##RgMax,                                                                   \
      k##pfx##BgMin,                                                                   \
      k##pfx##BgMax,                                                                   \
      k##pfx##GMin,                                                                    \
      k##pfx##GMax,                                                                    \
      k##pfx##NewW,                                                                    \
      k##pfx##PrevW,                                                                   \
      k##pfx##RScale,                                                                  \
      k##pfx##BScale,                                                                  \
      k##pfx##MinCounted,                                                              \
      k##pfx##AwbRefN ? (const Esp32p4IpaAwbRef *)k##pfx##AwbRef : kGenericAwb,        \
      k##pfx##AwbRefN ? k##pfx##AwbRefN : (uint8_t)5,                                  \
      (const Esp32p4IpaCcmRow *)k##pfx##Ccm,                                           \
      k##pfx##CcmN,                                                                    \
      k##pfx##LowLumaThr,                                                              \
      k##pfx##LowLumaM,                                                                \
      k##pfx##AeTarget,                                                                \
      k##pfx##AeLow,                                                                   \
      k##pfx##AeHigh,                                                                  \
      k##pfx##AeHiThr,                                                                 \
      k##pfx##AeHiReg,                                                                 \
      k##pfx##AcHz,                                                                    \
      k##pfx##Gamma,                                                                   \
      k##pfx##Sat,                                                                     \
      k##pfx##Contrast,                                                                \
      k##pfx##BfLevel,                                                                 \
      k##pfx##BfMat,                                                                   \
      k##pfx##ShH,                                                                     \
      k##pfx##ShL,                                                                     \
      k##pfx##ShHc,                                                                    \
      k##pfx##ShMc,                                                                    \
      k##pfx##ShMat,                                                                   \
      k##pfx##HasLsc ? &s_lsc_##pfx : nullptr,                                         \
      k##pfx##AeLoThr,                                                                 \
      k##pfx##AeLoReg,                                                                 \
      k##pfx##AeWt,                                                                    \
      k##pfx##IncR,                                                                    \
      k##pfx##DecR,                                                                    \
      k##pfx##EnvK,                                                                    \
      k##pfx##EnvSpN ? k##pfx##EnvSp : nullptr,                                        \
      k##pfx##EnvSpN,                                                                  \
      k##pfx##PwlN ? (const Esp32p4IpaPwl *)k##pfx##Pwl : nullptr,                     \
      k##pfx##PwlN,                                                                    \
      (const Esp32p4IpaBlc *)k##pfx##Blc,                                              \
      k##pfx##BlcN,                                                                    \
      k##pfx##BlcStretch,                                                              \
      k##pfx##AwbZoneN ? (const Esp32p4IpaAwbZone *)k##pfx##AwbZone : nullptr,         \
      k##pfx##AwbZoneN,                                                                \
  }

#define IPA_PROF(disp, pfx)                                                            \
  IPA_LSC(pfx)                                                                         \
  static const Esp32p4IpaProfile s_ipa_##pfx = IPA_FILL(disp, pfx);

IPA_PROF("SC2336", Sc2336)
IPA_PROF("OV5647", Ov5647)
IPA_PROF("OV2710", Ov2710)
IPA_PROF("OV9281", Ov9281)
IPA_PROF("SC202CS", Sc202cs)
IPA_PROF("SC1346", Sc1346)
IPA_PROF("SC035HGS", Sc035hgs)
static const Esp32p4IpaProfile s_ipa_Sc030iot = IPA_FILL("SC030IOT~SC035HGS", Sc035hgs);
IPA_PROF("SC2331", Sc2331)
IPA_PROF("GC2607", Gc2607)
IPA_PROF("OS02N10", Os02n10)
IPA_PROF("OS04C10", Os04c10)
IPA_PROF("STI2250", Sti2250)
IPA_PROF("MIRA220", Mira220)

static const Esp32p4IpaProfile kGeneric = {
    "generic",
    0.32f,
    0.97f,
    0.22f,
    0.80f,
    16,
    220,
    0.25f,
    0.75f,
    1.0f,
    1.0f,
    200,
    kGenericAwb,
    5,
    kGenericCcm,
    1,
    20.f,
    kIdent9,
    80,
    70,
    90,
    248,
    4,
    50,
    0.55f,
    128,
    128,
    4,
    kBfGeneric,
    20,
    4,
    2.0f,
    1.5f,
    kShGeneric,
    nullptr,
    13,
    5,
    kAeWtGeneric,
    0.32f,
    0.42f,
    0.f,
    nullptr,
    0,
    nullptr,
    0,
    kBlcGeneric,
    1,
    0,
    nullptr,
    0,
};

const Esp32p4IpaProfile *esp32p4_ipa_profile(esp32p4_cam_sensor_t id) {
  switch (id) {
    case ESP32P4_SENSOR_SC2336:
      return &s_ipa_Sc2336;
    case ESP32P4_SENSOR_OV5647:
      return &s_ipa_Ov5647;
    case ESP32P4_SENSOR_OV2710:
      return &s_ipa_Ov2710;
    case ESP32P4_SENSOR_OV9281:
      return &s_ipa_Ov9281;
    case ESP32P4_SENSOR_SC202CS:
      return &s_ipa_Sc202cs;
    case ESP32P4_SENSOR_SC1346:
      return &s_ipa_Sc1346;
    case ESP32P4_SENSOR_SC035HGS:
      return &s_ipa_Sc035hgs;
    case ESP32P4_SENSOR_SC030IOT:
      return &s_ipa_Sc030iot;
    case ESP32P4_SENSOR_SC2331:
      return &s_ipa_Sc2331;
    case ESP32P4_SENSOR_GC2607:
      return &s_ipa_Gc2607;
    case ESP32P4_SENSOR_OS02N10:
      return &s_ipa_Os02n10;
    case ESP32P4_SENSOR_OS04C10:
      return &s_ipa_Os04c10;
    case ESP32P4_SENSOR_STI2250:
      return &s_ipa_Sti2250;
    case ESP32P4_SENSOR_MIRA220:
      return &s_ipa_Mira220;
    default:
      return &kGeneric;
  }
}

void esp32p4_ipa_interp_ccm(const Esp32p4IpaProfile *p, float ct, float luma, float out[9]) {
  if (!p || !out) return;
  if (!p->ccm || p->ccm_n == 0) {
    for (int i = 0; i < 9; i++) out[i] = kIdent9[i];
    return;
  }
  if (p->ccm_n == 1 || ct <= p->ccm[0].ct) {
    for (int i = 0; i < 9; i++) out[i] = p->ccm[0].m[i];
  } else if (ct >= p->ccm[p->ccm_n - 1].ct) {
    const float *m = p->ccm[p->ccm_n - 1].m;
    for (int i = 0; i < 9; i++) out[i] = m[i];
  } else {
    uint8_t hi = 1;
    while (hi < p->ccm_n && p->ccm[hi].ct < ct) hi++;
    const uint8_t lo = (uint8_t)(hi - 1);
    const float span = p->ccm[hi].ct - p->ccm[lo].ct;
    const float t = span > 1.f ? (ct - p->ccm[lo].ct) / span : 0.f;
    for (int i = 0; i < 9; i++) {
      out[i] = p->ccm[lo].m[i] + (p->ccm[hi].m[i] - p->ccm[lo].m[i]) * t;
    }
  }
  if (p->low_luma_m && luma > 0.f && luma < p->low_luma_thr) {
    const float t = luma / (p->low_luma_thr > 1.f ? p->low_luma_thr : 1.f);
    for (int i = 0; i < 9; i++) {
      out[i] = p->low_luma_m[i] + (out[i] - p->low_luma_m[i]) * t;
    }
  }
}

float esp32p4_ipa_ct_from_rgbg(const Esp32p4IpaProfile *p, float rg, float bg) {
  if (!p || !p->awb_ref || p->awb_ref_n == 0) return 5000.f;
  float best = p->awb_ref[0].ct;
  float best_d = 1e9f;
  for (uint8_t i = 0; i < p->awb_ref_n; i++) {
    const float dr = rg - p->awb_ref[i].rg;
    const float db = bg - p->awb_ref[i].bg;
    const float d = dr * dr + db * db;
    if (d < best_d) {
      best_d = d;
      best = p->awb_ref[i].ct;
    }
  }
  return best;
}

uint16_t esp32p4_ipa_nearest_lsc(const Esp32p4IpaLsc *lsc, float ct) {
  if (!lsc || !lsc->ct || lsc->sets == 0) return 0;
  uint16_t best = 0;
  float best_d = 1e9f;
  for (uint8_t i = 0; i < lsc->sets; i++) {
    const float d = fabsf(ct - (float)lsc->ct[i]);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

void esp32p4_ipa_awb_attract(const Esp32p4IpaProfile *p, float *rg, float *bg) {
  if (!p || !rg || !bg || !p->awb_ref || p->awb_ref_n == 0) return;
  float best_d = 1e9f;
  uint8_t best_i = 0;
  for (uint8_t i = 0; i < p->awb_ref_n; i++) {
    const float dr = *rg - p->awb_ref[i].rg;
    const float db = *bg - p->awb_ref[i].bg;
    const float d = dr * dr + db * db;
    if (d < best_d) {
      best_d = d;
      best_i = i;
    }
  }
  const float r = p->awb_ref[best_i].radius;
  if (r <= 0.f) return;
  if (best_d <= r * r) {
    *rg = p->awb_ref[best_i].rg;
    *bg = p->awb_ref[best_i].bg;
  }
}

int esp32p4_ipa_awb_zone(const Esp32p4IpaProfile *p, float rg, float bg) {
  if (!p || !p->awb_zone || p->awb_zone_n == 0) return -1;
  for (uint8_t i = 0; i < p->awb_zone_n; i++) {
    const Esp32p4IpaAwbZone *z = &p->awb_zone[i];
    if (z->enabled < 0.5f) continue;
    if (rg >= z->rg_min && rg <= z->rg_max && bg >= z->bg_min && bg <= z->bg_max) {
      return (int)z->type;
    }
  }
  return -1;
}

float esp32p4_ipa_pwl_shift(const Esp32p4IpaProfile *p, float env_luma) {
  if (!p || !p->pwl || p->pwl_n == 0) return 0.f;
  if (env_luma <= p->pwl[0].env) return p->pwl[0].shift;
  if (env_luma >= p->pwl[p->pwl_n - 1].env) return p->pwl[p->pwl_n - 1].shift;
  uint8_t hi = 1;
  while (hi < p->pwl_n && p->pwl[hi].env < env_luma) hi++;
  const uint8_t lo = (uint8_t)(hi - 1);
  const float span = p->pwl[hi].env - p->pwl[lo].env;
  const float t = span > 1e-3f ? (env_luma - p->pwl[lo].env) / span : 0.f;
  return p->pwl[lo].shift + (p->pwl[hi].shift - p->pwl[lo].shift) * t;
}

bool esp32p4_ipa_blc_for_gain(const Esp32p4IpaProfile *p, float gain, uint16_t out[4]) {
  if (!out) return false;
  if (!p || !p->blc || p->blc_n == 0) {
    out[0] = out[1] = out[2] = out[3] = 16;
    return true;
  }
  uint8_t best_i = 0;
  float best_d = 1e9f;
  if (gain < 0.25f) gain = 0.25f;
  for (uint8_t i = 0; i < p->blc_n; i++) {
    const float d = fabsf(gain - p->blc[i].gain);
    if (d < best_d) {
      best_d = d;
      best_i = i;
    }
  }
  const Esp32p4IpaBlc *b = &p->blc[best_i];
  out[0] = (uint16_t)(b->tl + 0.5f);
  out[1] = (uint16_t)(b->tr + 0.5f);
  out[2] = (uint16_t)(b->bl + 0.5f);
  out[3] = (uint16_t)(b->br + 0.5f);
  return true;
}
