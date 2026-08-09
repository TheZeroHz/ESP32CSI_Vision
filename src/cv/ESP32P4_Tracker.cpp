#include "cv/ESP32P4_Tracker.h"

#include <stdio.h>
#include <string.h>

void ESP32P4_Tracker::reset() {
  memset(_tr, 0, sizeof(_tr));
  _next_id = 1;
}

int ESP32P4_Tracker::count() const {
  int n = 0;
  for (int i = 0; i < ESP32P4_TRACK_MAX; i++)
    if (_tr[i].active) n++;
  return n;
}

const esp32p4_track_t *ESP32P4_Tracker::track(int i) const {
  int seen = 0;
  for (int k = 0; k < ESP32P4_TRACK_MAX; k++) {
    if (!_tr[k].active) continue;
    if (seen == i) return &_tr[k];
    seen++;
  }
  return nullptr;
}

int ESP32P4_Tracker::update(const esp32p4_blob_t *dets, int n, int max_dist, int max_lost) {
  if (!dets) n = 0;
  if (n > 16) n = 16;
  if (max_dist < 8) max_dist = 8;

  bool det_used[16] = {};
  bool tr_matched[ESP32P4_TRACK_MAX] = {};

  // Greedy nearest-neighbor match (small N).
  for (int t = 0; t < ESP32P4_TRACK_MAX; t++) {
    if (!_tr[t].active) continue;
    int best = -1;
    int best_d2 = max_dist * max_dist + 1;
    for (int d = 0; d < n; d++) {
      if (det_used[d]) continue;
      int dx = dets[d].cx - _tr[t].cx;
      int dy = dets[d].cy - _tr[t].cy;
      int d2 = dx * dx + dy * dy;
      if (d2 < best_d2) {
        best_d2 = d2;
        best = d;
      }
    }
    if (best >= 0 && best_d2 <= max_dist * max_dist) {
      det_used[best] = true;
      tr_matched[t] = true;
      // Smooth centroid (simple EMA)
      _tr[t].cx = (_tr[t].cx * 2 + dets[best].cx) / 3;
      _tr[t].cy = (_tr[t].cy * 2 + dets[best].cy) / 3;
      _tr[t].w = dets[best].box.w;
      _tr[t].h = dets[best].box.h;
      _tr[t].area = dets[best].area;
      _tr[t].age++;
      _tr[t].lost = 0;
    }
  }

  for (int t = 0; t < ESP32P4_TRACK_MAX; t++) {
    if (!_tr[t].active) continue;
    if (tr_matched[t]) continue;
    _tr[t].lost++;
    if (_tr[t].lost > max_lost) _tr[t].active = false;
  }

  for (int d = 0; d < n; d++) {
    if (det_used[d]) continue;
    int slot = -1;
    for (int t = 0; t < ESP32P4_TRACK_MAX; t++) {
      if (!_tr[t].active) {
        slot = t;
        break;
      }
    }
    if (slot < 0) break;
    _tr[slot].id = _next_id++;
    if (_next_id > 9999) _next_id = 1;
    _tr[slot].cx = dets[d].cx;
    _tr[slot].cy = dets[d].cy;
    _tr[slot].w = dets[d].box.w;
    _tr[slot].h = dets[d].box.h;
    _tr[slot].area = dets[d].area;
    _tr[slot].age = 1;
    _tr[slot].lost = 0;
    _tr[slot].active = true;
  }

  return count();
}

void ESP32P4_Tracker::draw(uint16_t *img, int w, int h, uint16_t color) const {
  if (!img) return;
  for (int t = 0; t < ESP32P4_TRACK_MAX; t++) {
    if (!_tr[t].active) continue;
    esp32p4_blob_t b{};
    b.cx = _tr[t].cx;
    b.cy = _tr[t].cy;
    b.area = _tr[t].area;
    b.box.w = _tr[t].w > 0 ? _tr[t].w : 8;
    b.box.h = _tr[t].h > 0 ? _tr[t].h : 8;
    b.box.x = _tr[t].cx - b.box.w / 2;
    b.box.y = _tr[t].cy - b.box.h / 2;
    uint16_t c = (_tr[t].lost > 0) ? (uint16_t)0xFD20 : color;  // orange if coasting
    ESP32P4_Cv::drawBlob(img, w, h, b, c, 2);
    char lab[12];
    snprintf(lab, sizeof(lab), "ID%d", _tr[t].id);
    int tx = b.box.x;
    int ty = b.box.y - 12;
    if (ty < 2) ty = b.box.y + 2;
    ESP32P4_Cv::putText(img, w, h, tx, ty, lab, c, 1);
  }
}
