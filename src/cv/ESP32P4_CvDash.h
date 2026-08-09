#pragma once

/**
 * Live CV dashboard config + frame processor for MJPEG preview.
 * Modes exercise ESP32P4_Cv end-to-end (mask / blobs / edges / thresh / gray / blur).
 */

#include "cv/ESP32P4_Cv.h"

enum esp32p4_cv_mode_t : uint8_t {
  ESP32P4_CV_OFF = 0,
  ESP32P4_CV_BLOBS = 1,        // color/luma mask → morph → blobs
  ESP32P4_CV_MASK = 2,         // show binary mask
  ESP32P4_CV_EDGES = 3,        // edge map preview
  ESP32P4_CV_THRESH = 4,       // threshold preview
  ESP32P4_CV_GRAY = 5,         // gray preview
  ESP32P4_CV_BLUR = 6,         // blur preview
  ESP32P4_CV_EDGE_TRACK = 7,   // edges → objects → multi-ID tracking
};

enum esp32p4_cv_preset_t : uint8_t {
  ESP32P4_CV_PRESET_CUSTOM = 0,
  ESP32P4_CV_PRESET_ANY = 1,
  ESP32P4_CV_PRESET_RED = 2,
  ESP32P4_CV_PRESET_GREEN = 3,
  ESP32P4_CV_PRESET_BLUE = 4,
  ESP32P4_CV_PRESET_YELLOW = 5,
  ESP32P4_CV_PRESET_DARK = 6,
  ESP32P4_CV_PRESET_LIGHT = 7,
  ESP32P4_CV_PRESET_COINS = 8,
};

struct esp32p4_cv_dash_cfg_t {
  uint8_t mode = ESP32P4_CV_BLOBS;
  uint8_t preset = ESP32P4_CV_PRESET_DARK;
  esp32p4_hsv_t lo{0, 40, 40};
  esp32p4_hsv_t hi{179, 255, 255};
  uint8_t thr = 110;
  uint8_t edge_lo = 35;
  uint8_t edge_hi = 90;
  uint8_t erode_it = 0;
  uint8_t dilate_it = 1;
  uint16_t min_area = 25;
  uint16_t max_area = 20000;
  uint8_t border_ignore = 3;
  uint8_t track_dist = 80;  // max match distance (px)
  volatile int blobs = 0;
  volatile int tracks = 0;
  volatile int mask_px = 0;
  volatile int proc_ms = 0;
};

class ESP32P4_CvDash {
 public:
  static void applyPreset(esp32p4_cv_dash_cfg_t &cfg, uint8_t preset);
  /** Process one RGB565 stream frame in-place. Allocates PSRAM scratch as needed. */
  static void process(uint16_t *rgb, int w, int h, esp32p4_cv_dash_cfg_t &cfg);
  static void release();
};
