#include "cam/ESP32P4_SmartAe.h"

#include "cam/ESP32P4_Camera.h"
#include "img/ESP32P4_Img.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#if __has_include("cam/sensors/ov5647_sensor.h")
#include "cam/sensors/ov5647_sensor.h"
#define ESP32P4_SMART_AE_HAS_OV5647 1
#else
#define ESP32P4_SMART_AE_HAS_OV5647 0
#endif

#ifndef ESP32P4_SMART_AE_INTERVAL_MS
#define ESP32P4_SMART_AE_INTERVAL_MS 66
#endif

/** Quiet scenes can use more gain; clip scenes stay capped lower. */
#ifndef ESP32P4_SMART_AE_GAIN_CEIL_LO
#define ESP32P4_SMART_AE_GAIN_CEIL_LO 160
#endif
#ifndef ESP32P4_SMART_AE_GAIN_CEIL_HI
#define ESP32P4_SMART_AE_GAIN_CEIL_HI 220
#endif
#ifndef ESP32P4_SMART_AE_GAIN_FLOOR
#define ESP32P4_SMART_AE_GAIN_FLOOR 16
#endif

bool ESP32P4_SmartAe::begin(ESP32P4_Camera *cam) {
  _cam = cam;
  _inited = (_cam != nullptr);
  _have_iir = false;
  _last_run_ms = 0;
  _gain_floor = ESP32P4_SMART_AE_GAIN_FLOOR;
  _gain_ceil = ESP32P4_SMART_AE_GAIN_CEIL_LO;
  _gain = _gain_floor;
  _exp = 200;
  _peak = 0;
  if (!_inited) return false;

#if ESP32P4_SMART_AE_HAS_OV5647
  if (_cam->sensorType() == ESP32P4_SENSOR_OV5647) {
    _exp_max = ov5647_exposure_max_lines();
    if (_exp_max < 32) _exp_max = 980;
  }
#endif

  uint16_t exp = 0;
  if (_cam->getExposure(&exp) && exp >= 4) _exp = exp;
  _gain = _gain_floor;
  return true;
}

void ESP32P4_SmartAe::end() {
  _cam = nullptr;
  _inited = false;
  _en = false;
}

void ESP32P4_SmartAe::setTargetLuma(uint8_t luma) {
  if (luma < 40) luma = 40;
  if (luma > 160) luma = 160;
  _base_target = luma;
  setEvBias(_ev_bias);
}

void ESP32P4_SmartAe::setEvBias(int half_stops) {
  if (half_stops < -4) half_stops = -4;
  if (half_stops > 4) half_stops = 4;
  _ev_bias = half_stops;
  float t = (float)_base_target * powf(1.41421356f, (float)half_stops * 0.5f);
  if (t < 40.0f) t = 40.0f;
  if (t > 160.0f) t = 160.0f;
  _target = (uint8_t)(t + 0.5f);
}

void ESP32P4_SmartAe::applyManualMode() {
  if (!_cam) return;
  (void)_cam->setAEC(false);
  (void)_cam->setAGC(false);
  (void)_cam->setExposure(_exp);
  (void)_cam->setGain(_gain);
}

void ESP32P4_SmartAe::setEnabled(bool on) {
  if (!_inited || !_cam) {
    _en = false;
    return;
  }
  _en = on;
  if (on) {
    (void)_cam->setIspAe(false);
#if ESP32P4_SMART_AE_HAS_OV5647
    if (_cam->sensorType() == ESP32P4_SENSOR_OV5647) {
      uint16_t m = ov5647_exposure_max_lines();
      if (m >= 32) _exp_max = m;
    }
#endif
    uint16_t exp = 0;
    if (_cam->getExposure(&exp) && exp >= 4) _exp = exp;
    _gain_ceil = ESP32P4_SMART_AE_GAIN_CEIL_LO;
    _gain = _gain_floor;
    (void)_cam->setGainCeiling(ESP32P4_SMART_AE_GAIN_CEIL_HI);
    applyManualMode();
    _have_iir = false;
  } else {
    (void)_cam->setIspAe(true);
  }
}

float ESP32P4_SmartAe::meterFrame(const uint16_t *rgb, int w, int h, float *highlight_out,
                                  uint8_t *peak_out) const {
  const int gw = 48;
  const int gh = 27;
  const float cx0 = 0.30f * (float)(gw - 1);
  const float cx1 = 0.70f * (float)(gw - 1);
  const float cy0 = 0.30f * (float)(gh - 1);
  const float cy1 = 0.70f * (float)(gh - 1);

  double sum = 0.0;
  double wsum = 0.0;
  int hi = 0;
  int n = 0;
  uint8_t peak = 0;

  for (int gy = 0; gy < gh; gy++) {
    int y = (gy * (h - 1)) / (gh - 1);
    if (y < 0) y = 0;
    if (y >= h) y = h - 1;
    const uint16_t *row = rgb + (size_t)y * (size_t)w;
    for (int gx = 0; gx < gw; gx++) {
      int x = (gx * (w - 1)) / (gw - 1);
      uint8_t y8 = ESP32P4_Img::luma565(row[x]);
      float wt = 1.0f;
      if ((float)gx >= cx0 && (float)gx <= cx1 && (float)gy >= cy0 && (float)gy <= cy1) wt = 3.0f;
      sum += (double)y8 * (double)wt;
      wsum += (double)wt;
      if (y8 >= 250) hi++;
      if (y8 > peak) peak = y8;
      n++;
    }
  }

  if (highlight_out) *highlight_out = (n > 0) ? ((float)hi / (float)n) : 0.0f;
  if (peak_out) *peak_out = peak;
  if (wsum <= 0.0) return 100.0f;
  return (float)(sum / wsum);
}

void ESP32P4_SmartAe::stepControl(float meter, float highlight, uint8_t peak) {
  // True clip = sensor saturating. Faded dark paper has low peak — do NOT treat as clip.
  const bool true_clip = (peak >= 248) && (highlight > 0.008f);
  const bool dark_scene = (meter < 78.0f) && (peak < 210);
  const bool faded = (meter < 95.0f) && (peak < 190);

  // Dynamic gain ceiling: allow more ISO when the whole frame is dim.
  if (dark_scene || faded) {
    _gain_ceil = ESP32P4_SMART_AE_GAIN_CEIL_HI;
  } else if (true_clip) {
    _gain_ceil = 96;
  } else {
    _gain_ceil = ESP32P4_SMART_AE_GAIN_CEIL_LO;
  }

  float target = (float)_target;
  if (dark_scene) {
    // Lift target a bit so we chase usable midtones on dim QR paper.
    target = fminf(target + 18.0f, 125.0f);
  }

  if (true_clip) {
    float pen = highlight * 120.0f;
    if (pen > 45.0f) pen = 45.0f;
    target -= pen;
    if (target < 55.0f) target = 55.0f;

    if (_gain > _gain_floor) {
      int cut = 20 + (int)(highlight * 180.0f);
      if (cut > 56) cut = 56;
      int ng = (int)_gain - cut;
      if (ng < (int)_gain_floor) ng = (int)_gain_floor;
      _gain = (uint16_t)ng;
    }
    if (highlight > 0.04f) {
      int ne = (int)((float)_exp * 0.90f);
      if (ne < 4) ne = 4;
      _exp = (uint16_t)ne;
    }
    (void)_cam->setExposure(_exp);
    (void)_cam->setGain(_gain);
    return;
  }

  float err = target - meter;
  if (fabsf(err) < 4.0f && !dark_scene) return;

  // Dark/faded: open gain earlier (50% exposure). Bright: wait until ~85%.
  const uint16_t exp_gain_on =
      dark_scene ? (uint16_t)((_exp_max * 50u) / 100u) : (uint16_t)((_exp_max * 85u) / 100u);
  const bool allow_gain_up = !true_clip && (_exp >= exp_gain_on || (faded && _exp >= (_exp_max / 3)));

  if (err > 0.0f) {
    if (!allow_gain_up) {
      int step = (int)((float)_exp * (dark_scene ? 0.14f : 0.10f));
      if (step < 3) step = 3;
      step = (int)((float)step * fminf(fabsf(err) / 40.0f, 1.6f));
      uint32_t ne = (uint32_t)_exp + (uint32_t)step;
      if (ne > _exp_max) ne = _exp_max;
      _exp = (uint16_t)ne;
    } else {
      int gstep = dark_scene ? (8 + (int)(fabsf(err) * 0.35f)) : (4 + (int)(fabsf(err) * 0.18f));
      if (gstep > (dark_scene ? 20 : 10)) gstep = dark_scene ? 20 : 10;
      uint32_t ng = (uint32_t)_gain + (uint32_t)gstep;
      if (ng > _gain_ceil) ng = _gain_ceil;
      _gain = (uint16_t)ng;
    }
  } else {
    if (_gain > _gain_floor && (!dark_scene || err < -12.0f)) {
      int gstep = 10 + (int)(fabsf(err) * 0.5f);
      if (gstep > 36) gstep = 36;
      int ng = (int)_gain - gstep;
      if (ng < (int)_gain_floor) ng = (int)_gain_floor;
      _gain = (uint16_t)ng;
    } else {
      int step = (int)((float)_exp * 0.11f);
      if (step < 2) step = 2;
      step = (int)((float)step * fminf(fabsf(err) / 40.0f, 1.3f));
      int ne = (int)_exp - step;
      if (ne < 4) ne = 4;
      _exp = (uint16_t)ne;
    }
  }

  (void)_cam->setExposure(_exp);
  (void)_cam->setGain(_gain);
}

void ESP32P4_SmartAe::process(const uint16_t *rgb565, int w, int h) {
  if (!_en || !_cam || !rgb565 || w < 16 || h < 16) return;

  const uint32_t now = millis();
  if (_last_run_ms != 0 && (now - _last_run_ms) < ESP32P4_SMART_AE_INTERVAL_MS) return;
  _last_run_ms = now;

  const uint32_t t0 = millis();
  float hi = 0.0f;
  uint8_t peak = 0;
  float m = meterFrame(rgb565, w, h, &hi, &peak);
  _highlight = hi;
  _peak = peak;

  const bool dark = (m < 78.0f) && (peak < 210);
  float alpha = (hi > 0.02f && peak >= 248) ? 0.55f : (dark ? 0.40f : 0.28f);
  if (!_have_iir) {
    _meter_iir = m;
    _have_iir = true;
  } else {
    _meter_iir = (1.0f - alpha) * _meter_iir + alpha * m;
  }

  stepControl(_meter_iir, _highlight, _peak);
  _last_ms = millis() - t0;
}
