#pragma once

/**
 * ESP-DL pedestrian detect + person ReID (OSNet).
 * Weights: pedestrian_detect_pico_s8_v1.espdl + person_reid_feat_osn_s8_v1.espdl
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "vision/ESP32P4_VisionAi.h"

class ESP32P4_Reid {
 public:
  ESP32P4_Reid() = default;
  ~ESP32P4_Reid() { end(); }

  bool begin(const char *db_path = "/sdcard/reid.db");
  void end();
  bool ready() const { return _det != nullptr && _match != nullptr; }

  int run(const uint16_t *rgb565, int w, int h, esp32p4_reid_t *out, int max_out);
  int run(const camera_fb_t *fb, esp32p4_reid_t *out, int max_out);

  /** Enroll the largest person in the frame. Returns enrolled id, or -1. */
  int enroll(const uint16_t *rgb565, int w, int h);
  bool clearDb();
  bool deleteId(uint16_t id);
  int featCount() const;

  void setThresh(float thr);
  float thresh() const { return _thr; }

  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }

  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_reid_t *dets, int n,
                   uint16_t color = 0x07FF);

 private:
  bool ensureRgb(size_t pixels);
  bool toImg(const uint16_t *rgb565, int w, int h);

  void *_det = nullptr;
  void *_match = nullptr;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  int _img_w = 0, _img_h = 0;
  float _thr = 0.5f;
  int _last_ms = 0;
  int _last_n = 0;
};
