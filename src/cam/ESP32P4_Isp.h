#pragma once

/**
 * ESP32-P4 ISP 3A: Espressif IPA tables (CCM / LSC / BLC / AWB / AGC) applied
 * in-process. No V4L2. IPA AGC is the default; SmartAe is an opt-in fallback.
 */

#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "driver/isp.h"
#include "driver/isp_lsc.h"
#if __has_include("driver/isp_hist.h")
#include "driver/isp_hist.h"
#endif
#if __has_include("driver/isp_blc.h")
#include "driver/isp_blc.h"
#endif

struct Esp32p4IpaProfile;

class ESP32P4_Isp {
 public:
  bool begin(isp_proc_handle_t proc, uint16_t w, uint16_t h,
             esp32p4_cam_sensor_t sensor = ESP32P4_SENSOR_AUTO);
  void end();
  bool ready() const { return _proc != nullptr; }

  void process(ESP32P4_Camera *cam);

  bool setAwb(bool on);
  bool awbEnabled() const { return _awb_on; }
  bool setAe(bool on);
  bool aeEnabled() const { return _ae_on; }

  void setAeTarget(uint8_t luma);
  uint8_t aeTarget() const { return _ae_target; }
  /** EV bias in 1/2-stop units: -4 .. +4. Applied on top of IPA AGC target. */
  void setEvBias(int half_stops);
  int evBias() const { return _ev_bias; }
  void setAeLimits(uint16_t exp_min, uint16_t exp_max, uint16_t gain_min, uint16_t gain_max);
  /** 0 = use IPA ac_hz (often 0/off), 50 or 60 = snap exposure to 1/(2f). */
  void setAntiFlicker(uint8_t hz);
  uint8_t antiFlicker() const { return _ac_hz; }

  bool startAf();
  bool stopAf();
  bool afReady() const { return _af != nullptr; }
  uint32_t afScore();
  uint16_t afMaxPos() const;

  bool setBrightness(int8_t v);
  bool setContrast(uint8_t v);
  bool setSaturation(uint8_t v);
  bool setHue(uint16_t deg);
  /** Manual R/B vs green. 1024 = 1.0 (ESP_Video / V4L2_CID_*_BALANCE). Disables AWB. */
  bool setRedBalance(int32_t v1024);
  bool setBlueBalance(int32_t v1024);
  int32_t redBalance() const { return _rb_1024; }
  int32_t blueBalance() const { return _bb_1024; }
  /** 0 = off, 128 = IPA default, 255 = strong. */
  bool setSharpness(uint8_t v);
  uint8_t sharpness() const { return _sharpness; }
  /** Bayer NR strength 0–8 (ISP BF denoising_level). */
  bool setDenoise(uint8_t v);
  uint8_t denoise() const { return _denoise; }
  int8_t brightness() const { return _brightness; }
  uint8_t contrast() const { return _contrast; }
  uint8_t saturation() const { return _saturation; }
  uint16_t hue() const { return _hue; }

  float lastLuma() const { return _last_luma; }
  float envLuma() const { return _env_luma; }
  float redGain() const { return _rgain; }
  float blueGain() const { return _bgain; }
  float colorTemp() const { return _ct; }
  uint32_t whitePatches() const { return _white_n; }
  const char *profileName() const;
  bool lscEnabled() const { return _lsc_on; }
  bool blcEnabled() const { return _blc_on; }

#if __has_include("esp_video_isp_ioctl.h")
  /** Fill esp_video ISP stats for /dev/video20 (partial vs closed esp_ipa blob). */
  bool exportV4l2Stats(struct esp_video_isp_stats *out) const;
  /** V4L2_CID_USER_ESP_ISP_* blobs (CCM, gamma, BF, sharpen, demosaic, WB, LSC, AF, AWB, BLC). */
  bool importV4l2Cid(uint32_t id, const void *data, size_t size);
  bool exportV4l2Cid(uint32_t id, void *data, size_t size) const;
#endif

 private:
  bool startBlocks();
  bool applyColor();
  bool applyCcm();
  bool applyGamma();
  bool applySharpen();
  bool applyBf();
  bool applyLsc(float ct);
  bool applyBlc(float gain_lin);
  void freeLsc();
  bool startAwb();
  bool stopAwb();
  bool startAe();
  bool stopAe();
  bool startHist();
  bool stopHist();

  static bool onAwb(isp_awb_ctlr_t, const esp_isp_awb_evt_data_t *edata, void *ctx);
  static bool onAe(isp_ae_ctlr_t, const esp_isp_ae_env_detector_evt_data_t *edata, void *ctx);
#if __has_include("driver/isp_hist.h")
  static bool onHist(isp_hist_ctlr_t, const esp_isp_hist_evt_data_t *edata, void *ctx);
#endif

  isp_proc_handle_t _proc = nullptr;
  isp_awb_ctlr_t _awb = nullptr;
  isp_ae_ctlr_t _ae = nullptr;
  isp_af_ctlr_t _af = nullptr;
#if __has_include("driver/isp_hist.h")
  isp_hist_ctlr_t _hist = nullptr;
#endif
  const Esp32p4IpaProfile *_ipa = nullptr;
  esp32p4_cam_sensor_t _sensor = ESP32P4_SENSOR_AUTO;
  uint16_t _w = 0;
  uint16_t _h = 0;

  bool _awb_on = true;
  bool _ae_on = true;
  bool _ae_user = false;
  bool _manual_once = false;
  bool _lsc_on = false;
  bool _blc_on = false;
  bool _blc_skip = false;
  uint8_t _ae_target = 80;
  int _ev_bias = 0;
  uint16_t _exp_min = 8;
  uint16_t _exp_max = 1200;
  uint16_t _gain_min = 16;
  uint16_t _gain_max = 160;
  uint8_t _ac_hz = 0;

  int8_t _brightness = 0;
  uint8_t _contrast = 128;
  uint8_t _saturation = 128;
  uint16_t _hue = 0;
  uint8_t _sharpness = 128;
  uint8_t _denoise = 4;
  int32_t _rb_1024 = 1024;
  int32_t _bb_1024 = 1024;
  float _user_rgain = 1.f;
  float _user_bgain = 1.f;
  float _gamma = 0.55f;
  float _dm_grad = 1.5f;
  uint8_t _bf_mat[9]{};
  bool _bf_user = false;
  uint8_t _sh_mat[9]{};
  bool _sh_user = false;
  uint8_t _sh_h = 20;
  uint8_t _sh_l = 4;
  float _sh_hc = 2.f;
  float _sh_mc = 1.5f;
  uint16_t _blc_off[4]{16, 16, 16, 16};
  bool _blc_stretch = true;
  bool _ccm_user = false;
  float _ccm_m[9]{1.f, 0, 0, 0, 1.f, 0, 0, 0, 1.f};

  volatile uint32_t _white_n = 0;
  volatile uint32_t _sum_r = 0;
  volatile uint32_t _sum_g = 0;
  volatile uint32_t _sum_b = 0;
#if ISP_AWB_WINDOW_X_NUM > 0 && ISP_AWB_WINDOW_Y_NUM > 0
  volatile uint32_t _sw_n[ISP_AWB_WINDOW_X_NUM][ISP_AWB_WINDOW_Y_NUM]{};
  volatile uint32_t _sw_r[ISP_AWB_WINDOW_X_NUM][ISP_AWB_WINDOW_Y_NUM]{};
  volatile uint32_t _sw_g[ISP_AWB_WINDOW_X_NUM][ISP_AWB_WINDOW_Y_NUM]{};
  volatile uint32_t _sw_b[ISP_AWB_WINDOW_X_NUM][ISP_AWB_WINDOW_Y_NUM]{};
#endif
  volatile uint8_t _awb_fresh = 0;
  volatile uint8_t _ae_luma[ISP_AE_BLOCK_X_NUM][ISP_AE_BLOCK_Y_NUM]{};
  volatile uint8_t _ae_fresh = 0;
#if __has_include("driver/isp_hist.h")
  volatile uint32_t _hist_bin[16]{};
  volatile uint8_t _hist_fresh = 0;
#endif

  float _rgain = 1.0f;
  float _bgain = 1.0f;
  float _ct = 5000.f;
  float _last_luma = 0;
  float _env_luma = 0;
  uint16_t _exp = 200;
  uint16_t _gain = 16;
  uint16_t _lsc_idx = 0xffff;
  uint16_t _blc_key = 0xffff;
  uint32_t _last_awb_ms = 0;
  uint32_t _last_ae_ms = 0;

  esp_isp_lsc_gain_array_t _lsc_arr{};
  size_t _lsc_n = 0;
};
