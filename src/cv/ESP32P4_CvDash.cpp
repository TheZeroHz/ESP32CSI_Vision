#include "cv/ESP32P4_CvDash.h"

#include "cv/ESP32P4_Tracker.h"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "ppa/ESP32P4_Ppa.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t *s_a = nullptr;
static uint8_t *s_b = nullptr;
static uint8_t *s_c = nullptr;
static uint16_t *s_lab = nullptr;
static uint16_t *s_mag = nullptr;
static int s_w = 0, s_h = 0;
static ESP32P4_Tracker s_tracker;

static bool ensure(int w, int h) {
  if (w == s_w && h == s_h && s_a && s_b && s_c && s_lab && s_mag) return true;
  ESP32P4_CvDash::release();
  size_t px = (size_t)w * (size_t)h;
  s_a = (uint8_t *)esp32p4_psram_alloc(px);
  s_b = (uint8_t *)esp32p4_psram_alloc(px);
  s_c = (uint8_t *)esp32p4_psram_alloc(px);
  s_lab = (uint16_t *)esp32p4_psram_alloc(px * sizeof(uint16_t));
  s_mag = (uint16_t *)esp32p4_psram_alloc(px * sizeof(uint16_t));
  if (!s_a || !s_b || !s_c || !s_lab || !s_mag) {
    ESP32P4_CvDash::release();
    return false;
  }
  s_w = w;
  s_h = h;
  return true;
}

void ESP32P4_CvDash::release() {
  esp32p4_psram_free(s_a);
  esp32p4_psram_free(s_b);
  esp32p4_psram_free(s_c);
  esp32p4_psram_free(s_lab);
  esp32p4_psram_free(s_mag);
  s_a = s_b = s_c = nullptr;
  s_lab = s_mag = nullptr;
  s_w = s_h = 0;
  s_tracker.reset();
}

void ESP32P4_CvDash::applyPreset(esp32p4_cv_dash_cfg_t &cfg, uint8_t preset) {
  cfg.preset = preset;
  switch (preset) {
    case ESP32P4_CV_PRESET_ANY:
      cfg.lo = {0, 50, 50};
      cfg.hi = {179, 255, 255};
      cfg.erode_it = 1;
      cfg.dilate_it = 1;
      break;
    case ESP32P4_CV_PRESET_RED:
      cfg.lo = {160, 60, 50};
      cfg.hi = {10, 255, 255};
      cfg.erode_it = 1;
      cfg.dilate_it = 1;
      break;
    case ESP32P4_CV_PRESET_GREEN:
      cfg.lo = {35, 60, 50};
      cfg.hi = {85, 255, 255};
      cfg.erode_it = 1;
      cfg.dilate_it = 1;
      break;
    case ESP32P4_CV_PRESET_BLUE:
      cfg.lo = {90, 60, 50};
      cfg.hi = {130, 255, 255};
      cfg.erode_it = 1;
      cfg.dilate_it = 1;
      break;
    case ESP32P4_CV_PRESET_YELLOW:
      cfg.lo = {15, 60, 50};
      cfg.hi = {35, 255, 255};
      cfg.erode_it = 1;
      cfg.dilate_it = 1;
      break;
    case ESP32P4_CV_PRESET_DARK:
      cfg.thr = 0;
      cfg.erode_it = 1;
      cfg.dilate_it = 1;
      cfg.min_area = 0;
      cfg.border_ignore = 4;
      break;
    case ESP32P4_CV_PRESET_LIGHT:
      cfg.thr = 0;
      cfg.erode_it = 1;
      cfg.dilate_it = 1;
      cfg.min_area = 0;
      cfg.border_ignore = 4;
      break;
    case ESP32P4_CV_PRESET_COINS:
      cfg.thr = 0;
      cfg.erode_it = 1;
      cfg.dilate_it = 2;
      cfg.min_area = 0;
      cfg.border_ignore = 4;
      break;
    default:
      break;
  }
}

static void clearBorder(uint8_t *m, int w, int h, int b) {
  if (b <= 0 || b * 2 >= w || b * 2 >= h) return;
  for (int y = 0; y < b; y++) {
    memset(m + y * w, 0, (size_t)w);
    memset(m + (h - 1 - y) * w, 0, (size_t)w);
  }
  for (int y = b; y < h - b; y++) {
    memset(m + y * w, 0, (size_t)b);
    memset(m + y * w + (w - b), 0, (size_t)b);
  }
}

static void clearTop(uint8_t *m, int w, int h, int rows) {
  if (rows <= 0) return;
  if (rows > h) rows = h;
  memset(m, 0, (size_t)w * (size_t)rows);
}

static void grayToRgb(const uint8_t *g, uint16_t *rgb, int n) {
  for (int i = 0; i < n; i++) {
    uint8_t v = g[i];
    rgb[i] = (uint16_t)(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3));
  }
}

static void maskToRgb(const uint8_t *m, uint16_t *rgb, int n) {
  const uint16_t on = 0x07E0;
  const uint16_t off = 0x1082;
  for (int i = 0; i < n; i++) rgb[i] = m[i] ? on : off;
}

static void edgesToRgb(const uint8_t *e, uint16_t *rgb, int n) {
  for (int i = 0; i < n; i++) {
    uint8_t v = e[i];
    if (v >= 200) rgb[i] = 0x07FF;
    else if (v >= 100) rgb[i] = 0x001F;
    else rgb[i] = 0x0841;
  }
}

static int countMask(const uint8_t *m, int n) {
  int c = 0;
  for (int i = 0; i < n; i++)
    if (m[i]) c++;
  return c;
}

static bool isLumaPreset(uint8_t preset) {
  return preset == ESP32P4_CV_PRESET_DARK || preset == ESP32P4_CV_PRESET_LIGHT ||
         preset == ESP32P4_CV_PRESET_COINS;
}

static uint8_t robustDarkThr(const uint8_t *gray, int n, uint8_t ots) {
  uint64_t sum = 0, sum2 = 0;
  for (int i = 0; i < n; i++) {
    uint32_t g = gray[i];
    sum += g;
    sum2 += g * g;
  }
  double mean = (double)sum / (double)n;
  double var = (double)sum2 / (double)n - mean * mean;
  if (var < 0) var = 0;
  double stdv = sqrt(var);
  // Sit between paper (high) and object (lower): mean - k*std, floored.
  int robust = (int)(mean - 0.45 * stdv);
  if (robust < 55) robust = 55;
  if (robust > 200) robust = 200;

  // Washed-out / glare scenes make Otsu collapse near 0 (seen as T7) — ignore it.
  if (ots < 40 || ots > (uint8_t)mean) return (uint8_t)robust;
  return (uint8_t)(((int)ots + robust) / 2);
}

/**
 * Classic OpenCV dark-object pipeline (fixed for bright/glare scenes):
 *   gray → blur → robust thr (+ adaptive for coins) → open → close
 */
static void buildMaskDarkLight(uint16_t *rgb, int w, int h, esp32p4_cv_dash_cfg_t &cfg,
                               uint8_t *out) {
  const int n = w * h;
  const bool dark = (cfg.preset != ESP32P4_CV_PRESET_LIGHT);
  const bool coins = (cfg.preset == ESP32P4_CV_PRESET_COINS);

  ESP32P4_Cv::toGray(rgb, w, h, s_a);
  ESP32P4_Cv::blur3x3(s_a, w, h, s_b);

  uint8_t thr = cfg.thr;
  if (thr == 0) {
    uint8_t ots = ESP32P4_Cv::otsu(s_b, (size_t)n);
    thr = dark ? robustDarkThr(s_b, n, ots) : (uint8_t)(255 - robustDarkThr(s_b, n, (uint8_t)(255 - ots)));
  }
  if (thr < 40) thr = 40;

  ESP32P4_Cv::threshold(s_b, w, h, s_c, thr,
                        dark ? ESP32P4_THRESH_BINARY_INV : ESP32P4_THRESH_BINARY);

  if (coins || dark) {
    static uint32_t *s_ii = nullptr;
    static size_t s_ii_cap = 0;
    size_t need = (size_t)(w + 1) * (size_t)(h + 1);
    if (s_ii_cap < need) {
      esp32p4_psram_free(s_ii);
      s_ii = (uint32_t *)esp32p4_psram_alloc(need * sizeof(uint32_t));
      s_ii_cap = s_ii ? need : 0;
    }
    ESP32P4_Cv::adaptiveThreshold(s_b, w, h, s_a, coins ? 31 : 21, coins ? 6 : 8, dark, s_ii);
    for (int i = 0; i < n; i++) {
      if (s_a[i]) s_c[i] = 255;
    }
  }

  int open_it = cfg.erode_it > 0 ? cfg.erode_it : 1;
  ESP32P4_Cv::morphologyOpen(s_c, w, h, s_a, open_it);
  int close_it = cfg.dilate_it > 0 ? cfg.dilate_it : 1;
  if (coins && close_it < 2) close_it = 2;
  ESP32P4_Cv::morphologyClose(s_a, w, h, out, close_it);

  clearBorder(out, w, h, cfg.border_ignore > 0 ? cfg.border_ignore : 4);
  clearTop(out, w, h, 28);
  cfg.mask_px = countMask(out, n);
  cfg.thr = thr;
}

static void buildMaskColor(uint16_t *rgb, int w, int h, esp32p4_cv_dash_cfg_t &cfg, uint8_t *out) {
  const int n = w * h;
  ESP32P4_Cv::inRangeHsv(rgb, w, h, s_a, cfg.lo, cfg.hi);
  int open_it = cfg.erode_it > 0 ? cfg.erode_it : 1;
  ESP32P4_Cv::morphologyOpen(s_a, w, h, s_b, open_it);
  int close_it = cfg.dilate_it > 0 ? cfg.dilate_it : 1;
  ESP32P4_Cv::morphologyClose(s_b, w, h, out, close_it);
  clearBorder(out, w, h, cfg.border_ignore > 0 ? cfg.border_ignore : 4);
  clearTop(out, w, h, 28);
  cfg.mask_px = countMask(out, n);
}

static void buildMask(uint16_t *rgb, int w, int h, esp32p4_cv_dash_cfg_t &cfg, uint8_t *out) {
  if (isLumaPreset(cfg.preset)) buildMaskDarkLight(rgb, w, h, cfg, out);
  else buildMaskColor(rgb, w, h, cfg, out);
}

/** Circle-oriented blob filter (aspect, fill, circularity, relative size). */
static int filterBlobsAccurate(esp32p4_blob_t *blobs, int n, int w, int h,
                               const esp32p4_cv_dash_cfg_t &cfg) {
  const int frame_area = w * h;
  int min_a = (int)cfg.min_area;
  if (min_a <= 0) min_a = frame_area / 500;  // ~256 @ 400x320
  if (min_a < 60) min_a = 60;
  int max_a = (int)cfg.max_area;
  if (max_a <= 0) max_a = frame_area / 3;

  const int margin = (cfg.border_ignore > 0 ? cfg.border_ignore : 4) + 2;
  const int top_cut = 28;

  esp32p4_blob_t tmp[16];
  int m = 0;
  for (int i = 0; i < n && m < 16; i++) {
    const esp32p4_blob_t &b = blobs[i];
    if (b.area < min_a || b.area > max_a) continue;
    if (b.box.y < top_cut) continue;
    if (b.box.x <= margin || b.box.y <= margin) continue;
    if (b.box.x + b.box.w >= w - margin) continue;
    if (b.box.y + b.box.h >= h - margin) continue;

    int bw = b.box.w > 0 ? b.box.w : 1;
    int bh = b.box.h > 0 ? b.box.h : 1;
    float aspect = bw > bh ? (float)bw / (float)bh : (float)bh / (float)bw;
    if (aspect > 1.65f) continue;  // reject pills / bars; allow slight oval coins

    float fill = (float)b.area / (float)(bw * bh);
    if (fill < 0.40f) continue;

    float r = 0.5f * (float)(bw > bh ? bw : bh);
    float circ = (float)b.area / (3.1415926f * r * r);
    if (circ < 0.42f || circ > 1.40f) continue;

    tmp[m++] = b;
  }

  if (m <= 1) {
    for (int i = 0; i < m; i++) blobs[i] = tmp[i];
    return m;
  }

  // Sort by area descending
  for (int i = 0; i < m; i++) {
    for (int j = i + 1; j < m; j++) {
      if (tmp[j].area > tmp[i].area) {
        esp32p4_blob_t t = tmp[i];
        tmp[i] = tmp[j];
        tmp[j] = t;
      }
    }
  }

  // Drop tiny outliers vs largest (kills residual speckles)
  const int floor_a = (int)(tmp[0].area * 0.18f);
  int out = 0;
  for (int i = 0; i < m; i++) {
    if (tmp[i].area < floor_a) continue;
    blobs[out++] = tmp[i];
  }
  return out;
}

/** Edge clusters → object boxes (fill is low; use bbox size / aspect). */
static int filterEdgeObjects(esp32p4_blob_t *blobs, int n, int w, int h,
                             const esp32p4_cv_dash_cfg_t &cfg) {
  const int frame_area = w * h;
  int min_box = frame_area / 400;
  if (min_box < 80) min_box = 80;
  int max_box = frame_area / 2;
  const int margin = (cfg.border_ignore > 0 ? cfg.border_ignore : 4) + 2;
  const int top_cut = 24;

  int out = 0;
  for (int i = 0; i < n && out < 16; i++) {
    esp32p4_blob_t &b = blobs[i];
    int box_a = b.box.w * b.box.h;
    if (box_a < min_box || box_a > max_box) continue;
    if (b.box.y < top_cut) continue;
    if (b.box.x <= margin || b.box.y <= margin) continue;
    if (b.box.x + b.box.w >= w - margin) continue;
    if (b.box.y + b.box.h >= h - margin) continue;
    int bw = b.box.w > 0 ? b.box.w : 1;
    int bh = b.box.h > 0 ? b.box.h : 1;
    float aspect = bw > bh ? (float)bw / (float)bh : (float)bh / (float)bw;
    if (aspect > 2.2f) continue;
    // Prefer somewhat filled edge clusters (after dilate)
    float fill = (float)b.area / (float)box_a;
    if (fill < 0.08f) continue;
    blobs[out++] = b;
  }
  return out;
}

/** Edges → dilate/close → connected components → tracker (half-res + PPA gray). */
static void scaleBlobs2x(esp32p4_blob_t *blobs, int n) {
  for (int i = 0; i < n; i++) {
    blobs[i].cx *= 2;
    blobs[i].cy *= 2;
    blobs[i].box.x *= 2;
    blobs[i].box.y *= 2;
    blobs[i].box.w *= 2;
    blobs[i].box.h *= 2;
    blobs[i].area *= 4;
  }
}

static void runEdgeTrack(uint16_t *rgb, int w, int h, esp32p4_cv_dash_cfg_t &cfg) {
  // OpenCV / esp32-opencv style: detect on pyramid level 1 (½ res) — ~4× fewer pixels.
  const int cw = w / 2;
  const int ch = h / 2;
  if (cw < 40 || ch < 40) {
    // Tiny frames: stay full-res
    ESP32P4_Cv::toGray(rgb, w, h, s_a);
    ESP32P4_Cv::blur3x3(s_a, w, h, s_b);
    uint8_t elo = cfg.edge_lo ? cfg.edge_lo : 35;
    uint8_t ehi = cfg.edge_hi ? cfg.edge_hi : 90;
    if (ehi < elo + 10) ehi = (uint8_t)(elo + 10);
    ESP32P4_Cv::edges(s_b, w, h, s_a, elo, ehi, s_mag);
    for (int i = 0; i < w * h; i++) s_a[i] = (s_a[i] >= 200) ? 255 : 0;
    int dil = cfg.dilate_it > 0 ? cfg.dilate_it : 3;
    if (dil < 2) dil = 2;
    ESP32P4_Cv::dilate(s_a, w, h, s_c, dil);
    ESP32P4_Cv::morphologyClose(s_c, w, h, s_b, 2);
    clearBorder(s_b, w, h, cfg.border_ignore > 0 ? cfg.border_ignore : 4);
    clearTop(s_b, w, h, 24);
    cfg.mask_px = countMask(s_b, w * h);
    int min_a = (int)cfg.min_area;
    if (min_a <= 0) min_a = (w * h) / 800;
    if (min_a < 40) min_a = 40;
    esp32p4_blob_t blobs[16];
    int bn = ESP32P4_Cv::findBlobs(s_b, w, h, blobs, 16, min_a, s_lab);
    bn = filterEdgeObjects(blobs, bn, w, h, cfg);
    cfg.blobs = bn;
    cfg.tracks = s_tracker.update(blobs, bn, cfg.track_dist ? cfg.track_dist : 80, 15);
    s_tracker.draw(rgb, w, h, 0x07FF);
    return;
  }

  // PPA: RGB565 → GRAY8 @ ½ in one HW pass
  ESP32P4_Cv::toGrayScale(rgb, w, h, s_a, cw, ch);
  ESP32P4_Cv::blur3x3(s_a, cw, ch, s_b);
  uint8_t elo = cfg.edge_lo ? cfg.edge_lo : 35;
  uint8_t ehi = cfg.edge_hi ? cfg.edge_hi : 90;
  if (ehi < elo + 10) ehi = (uint8_t)(elo + 10);
  ESP32P4_Cv::edges(s_b, cw, ch, s_a, elo, ehi, s_mag);

  const int cn = cw * ch;
  for (int i = 0; i < cn; i++) s_a[i] = (s_a[i] >= 200) ? 255 : 0;
  int dil = cfg.dilate_it > 0 ? cfg.dilate_it : 3;
  if (dil < 2) dil = 2;
  ESP32P4_Cv::dilate(s_a, cw, ch, s_c, dil);
  ESP32P4_Cv::morphologyClose(s_c, cw, ch, s_b, 2);
  clearBorder(s_b, cw, ch, (cfg.border_ignore > 0 ? cfg.border_ignore : 4) / 2);
  clearTop(s_b, cw, ch, 12);
  cfg.mask_px = countMask(s_b, cn) * 4;

  int min_a = (int)cfg.min_area;
  if (min_a <= 0) min_a = cn / 800;
  if (min_a < 20) min_a = 20;
  // min_area is full-res-ish from UI — scale down for work res
  if ((int)cfg.min_area > 0) min_a = (int)cfg.min_area / 4;
  if (min_a < 20) min_a = 20;

  esp32p4_blob_t blobs[16];
  int bn = ESP32P4_Cv::findBlobs(s_b, cw, ch, blobs, 16, min_a, s_lab);
  bn = filterEdgeObjects(blobs, bn, cw, ch, cfg);
  scaleBlobs2x(blobs, bn);
  cfg.blobs = bn;

  int max_dist = cfg.track_dist ? cfg.track_dist : 80;
  cfg.tracks = s_tracker.update(blobs, bn, max_dist, 15);
  s_tracker.draw(rgb, w, h, 0x07FF);
}

void ESP32P4_CvDash::process(uint16_t *rgb, int w, int h, esp32p4_cv_dash_cfg_t &cfg) {
  if (!rgb || w <= 0 || h <= 0 || cfg.mode == ESP32P4_CV_OFF) {
    cfg.blobs = 0;
    cfg.tracks = 0;
    cfg.mask_px = 0;
    cfg.proc_ms = 0;
    return;
  }
  if (!ensure(w, h)) return;

  uint32_t t0 = millis();
  const int n = w * h;
  cfg.blobs = 0;
  cfg.tracks = 0;

  switch (cfg.mode) {
    case ESP32P4_CV_GRAY:
      ESP32P4_Cv::toGray(rgb, w, h, s_a);
      grayToRgb(s_a, rgb, n);
      break;

    case ESP32P4_CV_BLUR:
      ESP32P4_Cv::toGray(rgb, w, h, s_a);
      ESP32P4_Cv::blur3x3(s_a, w, h, s_b);
      grayToRgb(s_b, rgb, n);
      break;

    case ESP32P4_CV_THRESH:
      buildMask(rgb, w, h, cfg, s_b);
      grayToRgb(s_b, rgb, n);
      break;

    case ESP32P4_CV_EDGES:
      ESP32P4_Cv::toGray(rgb, w, h, s_a);
      ESP32P4_Cv::blur3x3(s_a, w, h, s_b);
      ESP32P4_Cv::edges(s_b, w, h, s_a, cfg.edge_lo, cfg.edge_hi, s_mag);
      edgesToRgb(s_a, rgb, n);
      cfg.mask_px = countMask(s_a, n);
      break;

    case ESP32P4_CV_EDGE_TRACK:
      runEdgeTrack(rgb, w, h, cfg);
      break;

    case ESP32P4_CV_MASK:
      buildMask(rgb, w, h, cfg, s_b);
      maskToRgb(s_b, rgb, n);
      break;

    case ESP32P4_CV_BLOBS:
    default: {
      const bool half = isLumaPreset(cfg.preset) && (w >= 160) && (h >= 128);
      if (half) {
        const int cw = w / 2, ch = h / 2;
        // Build mask at ½ via temporary RGB downsample (PPA scale) then luma pipeline
        static uint16_t *s_half = nullptr;
        static int s_hw = 0, s_hh = 0;
        if (s_hw != cw || s_hh != ch || !s_half) {
          esp32p4_psram_free(s_half);
          s_half = (uint16_t *)esp32p4_psram_alloc((size_t)cw * (size_t)ch * 2);
          s_hw = s_half ? cw : 0;
          s_hh = s_half ? ch : 0;
        }
        bool ok = s_half && ESP32P4_Ppa::cv().scaleRgb565(rgb, w, h, s_half, cw, ch);
        if (!ok && s_half) {
          ESP32P4_Img::downsample2x565(rgb, w, h, s_half);
          ok = true;
        }
        if (ok) {
          // Temporarily point scratch at half dims by running mask into s_b (sized for full)
          buildMask(s_half, cw, ch, cfg, s_b);
          int min_a = (int)cfg.min_area;
          if (min_a <= 0) min_a = (cw * ch) / 500;
          else min_a = min_a / 4;
          if (min_a < 20) min_a = 20;
          esp32p4_blob_t blobs[16];
          int bn = ESP32P4_Cv::findBlobs(s_b, cw, ch, blobs, 16, min_a, s_lab);
          bn = filterBlobsAccurate(blobs, bn, cw, ch, cfg);
          scaleBlobs2x(blobs, bn);
          cfg.blobs = bn;
          cfg.tracks = s_tracker.update(blobs, bn, cfg.track_dist ? cfg.track_dist : 80, 12);
          if (cfg.tracks > 0) s_tracker.draw(rgb, w, h, 0xAFE0);
          else {
            const uint16_t lime = 0xAFE0;
            for (int i = 0; i < bn; i++) ESP32P4_Cv::drawBlob(rgb, w, h, blobs[i], lime, 2);
          }
          break;
        }
      }
      buildMask(rgb, w, h, cfg, s_b);
      int min_a = (int)cfg.min_area;
      if (min_a <= 0) min_a = n / 500;
      if (min_a < 60) min_a = 60;
      esp32p4_blob_t blobs[16];
      int bn = ESP32P4_Cv::findBlobs(s_b, w, h, blobs, 16, min_a, s_lab);
      bn = filterBlobsAccurate(blobs, bn, w, h, cfg);
      cfg.blobs = bn;
      cfg.tracks = s_tracker.update(blobs, bn, cfg.track_dist ? cfg.track_dist : 80, 12);
      if (cfg.tracks > 0) s_tracker.draw(rgb, w, h, 0xAFE0);
      else {
        const uint16_t lime = 0xAFE0;
        for (int i = 0; i < bn; i++) ESP32P4_Cv::drawBlob(rgb, w, h, blobs[i], lime, 2);
      }
      break;
    }
  }

  char hud[56];
  if (cfg.mode == ESP32P4_CV_EDGE_TRACK) {
    snprintf(hud, sizeof(hud), "EDGE TR%d DET%d P%d", (int)cfg.tracks, (int)cfg.blobs,
             (int)cfg.mask_px);
  } else {
    snprintf(hud, sizeof(hud), "M%d B%d TR%d T%d", (int)cfg.mode, (int)cfg.blobs, (int)cfg.tracks,
             (int)cfg.thr);
  }
  ESP32P4_Cv::putText(rgb, w, h, 6, 6, hud, 0xF800, 2);

  cfg.proc_ms = (int)(millis() - t0);
}
