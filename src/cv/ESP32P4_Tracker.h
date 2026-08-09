#pragma once

/**
 * Lightweight multi-object centroid tracker (edge / blob detections → stable IDs).
 */

#include "cv/ESP32P4_Cv.h"

#ifndef ESP32P4_TRACK_MAX
#define ESP32P4_TRACK_MAX 12
#endif

struct esp32p4_track_t {
  int id;
  int cx, cy;
  int w, h;
  int area;
  int age;    // frames seen
  int lost;   // consecutive misses
  bool active;
};

class ESP32P4_Tracker {
 public:
  void reset();
  /** Associate detections with tracks. max_dist in pixels. Returns active track count. */
  int update(const esp32p4_blob_t *dets, int n, int max_dist = 80, int max_lost = 12);
  int count() const;
  const esp32p4_track_t *track(int i) const;

  /** Draw boxes + ID labels onto RGB565. */
  void draw(uint16_t *img, int w, int h, uint16_t color = 0x07FF) const;

 private:
  esp32p4_track_t _tr[ESP32P4_TRACK_MAX]{};
  int _next_id = 1;
};
