#pragma once

/**
 * Phone-inspired software auto-exposure for ESP32-P4 CSI RGB565.
 * Disables crude on-sensor AEC/AGC and drives manual exposure then gain
 * from center-weighted luma metering + highlight penalty.
 */

#include <stdint.h>

class ESP32P4_Camera;

class ESP32P4_SmartAe {
 public:
  bool begin(ESP32P4_Camera *cam);
  void end();

  bool enabled() const { return _en; }
  void setEnabled(bool on);

  /** EV bias in 1/2-stop units: -4 .. +4 (maps target luma). */
  void setEvBias(int half_stops);
  int evBias() const { return _ev_bias; }

  void setTargetLuma(uint8_t luma);
  uint8_t targetLuma() const { return _target; }

  /**
   * Meter RGB565 and update sensor if due (~10–15 Hz).
   * Safe to call from the MJPEG worker; returns quickly when rate-limited.
   */
  void process(const uint16_t *rgb565, int w, int h);

  float lastMeter() const { return _meter_iir; }
  float lastHighlight() const { return _highlight; }
  uint8_t lastPeak() const { return _peak; }
  uint16_t lastExposure() const { return _exp; }
  uint16_t lastGain() const { return _gain; }
  uint32_t lastMs() const { return _last_ms; }

 private:
  void applyManualMode();
  float meterFrame(const uint16_t *rgb, int w, int h, float *highlight_out, uint8_t *peak_out) const;
  void stepControl(float meter, float highlight, uint8_t peak);

  ESP32P4_Camera *_cam = nullptr;
  bool _en = false;
  bool _inited = false;
  int _ev_bias = 0;
  uint8_t _base_target = 105;
  uint8_t _target = 105;

  float _meter_iir = 105.0f;
  float _highlight = 0.0f;
  uint8_t _peak = 0;
  uint16_t _exp = 200;
  uint16_t _gain = 16;
  uint16_t _exp_max = 980;
  uint16_t _gain_floor = 16;
  uint16_t _gain_ceil = 160;

  uint32_t _last_run_ms = 0;
  uint32_t _last_ms = 0;
  bool _have_iir = false;
};
