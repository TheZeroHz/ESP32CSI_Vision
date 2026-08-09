#include "cv/ESP32P4_Cv.h"

#include "mem/ESP32P4_Psram.h"
#include "ppa/ESP32P4_Ppa.h"

#include <stdlib.h>
#include <string.h>

static void clampRoi(int w, int h, const esp32p4_rect_t *roi, int &x0, int &y0, int &x1, int &y1) {
  if (roi) {
    x0 = roi->x;
    y0 = roi->y;
    x1 = roi->x + roi->w;
    y1 = roi->y + roi->h;
  } else {
    x0 = 0;
    y0 = 0;
    x1 = w;
    y1 = h;
  }
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > w) x1 = w;
  if (y1 > h) y1 = h;
}

static void toGraySw(const uint16_t *src, size_t n, uint8_t *dst) {
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    dst[i] = ESP32P4_Img::luma565(src[i]);
    dst[i + 1] = ESP32P4_Img::luma565(src[i + 1]);
    dst[i + 2] = ESP32P4_Img::luma565(src[i + 2]);
    dst[i + 3] = ESP32P4_Img::luma565(src[i + 3]);
  }
  for (; i < n; i++) dst[i] = ESP32P4_Img::luma565(src[i]);
}

bool ESP32P4_Cv::toGray(const uint16_t *src, int w, int h, uint8_t *dst, const esp32p4_rect_t *roi) {
  if (!src || !dst || w <= 0 || h <= 0) return false;
  if (!roi) {
    if (ESP32P4_Ppa::cv().rgb565ToGray(src, w, h, dst)) return true;
    toGraySw(src, (size_t)w * (size_t)h, dst);
    return true;
  }
  int x0, y0, x1, y1;
  clampRoi(w, h, roi, x0, y0, x1, y1);
  int dw = x1 - x0, dh = y1 - y0;
  for (int y = 0; y < dh; y++) {
    const uint16_t *row = src + (y0 + y) * w + x0;
    uint8_t *drow = dst + y * dw;
    for (int x = 0; x < dw; x++) drow[x] = ESP32P4_Img::luma565(row[x]);
  }
  return true;
}

bool ESP32P4_Cv::downsample2x(const uint8_t *src, int sw, int sh, uint8_t *dst) {
  if (!src || !dst || sw < 2 || sh < 2) return false;
  int dw = sw / 2, dh = sh / 2;
  for (int y = 0; y < dh; y++) {
    const uint8_t *row = src + (y * 2) * sw;
    uint8_t *drow = dst + y * dw;
    for (int x = 0; x < dw; x++) drow[x] = row[x * 2];
  }
  return true;
}

bool ESP32P4_Cv::toGrayScale(const uint16_t *src, int sw, int sh, uint8_t *dst, int dw, int dh) {
  if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return false;
  if (ESP32P4_Ppa::cv().rgb565ToGrayScale(src, sw, sh, dst, dw, dh)) return true;
  // SW fallback: full gray then nearest scale
  uint8_t *full = (uint8_t *)esp32p4_psram_alloc((size_t)sw * (size_t)sh);
  if (!full) return false;
  toGraySw(src, (size_t)sw * (size_t)sh, full);
  for (int y = 0; y < dh; y++) {
    int sy = y * sh / dh;
    const uint8_t *row = full + sy * sw;
    uint8_t *drow = dst + y * dw;
    for (int x = 0; x < dw; x++) drow[x] = row[x * sw / dw];
  }
  esp32p4_psram_free(full);
  return true;
}

/** Separable 3×3 box blur — ~3× fewer ops than 3×3 nested loops (esp32-opencv style). */
bool ESP32P4_Cv::blur3x3(const uint8_t *src, int w, int h, uint8_t *dst, const esp32p4_rect_t *roi) {
  if (!src || !dst || w < 3 || h < 3) return false;
  if (roi) {
    // ROI path: keep simple full-kernel (rare in dashboard)
    int x0, y0, x1, y1;
    clampRoi(w, h, roi, x0, y0, x1, y1);
    const int dw = x1 - x0, dh = y1 - y0;
    uint8_t *tmp = (uint8_t *)esp32p4_psram_alloc((size_t)dw * (size_t)dh);
    if (!tmp) return false;
    for (int y = 0; y < dh; y++) memcpy(tmp + y * dw, src + (y0 + y) * w + x0, (size_t)dw);
    bool ok = blur3x3(tmp, dw, dh, dst, nullptr);
    esp32p4_psram_free(tmp);
    return ok;
  }

  static uint8_t *rowbuf = nullptr;
  static int rowbuf_w = 0;
  if (rowbuf_w < w) {
    esp32p4_psram_free(rowbuf);
    rowbuf = (uint8_t *)esp32p4_psram_alloc((size_t)w * 3);
    rowbuf_w = rowbuf ? w : 0;
    if (!rowbuf) return false;
  }

  // Horizontal pass into dst, then vertical in-place using 3-row ring.
  for (int y = 0; y < h; y++) {
    const uint8_t *s = src + y * w;
    uint8_t *d = dst + y * w;
    d[0] = (uint8_t)(((uint32_t)s[0] + s[0] + s[1]) / 3);
    for (int x = 1; x < w - 1; x++)
      d[x] = (uint8_t)(((uint32_t)s[x - 1] + s[x] + s[x + 1]) / 3);
    d[w - 1] = (uint8_t)(((uint32_t)s[w - 2] + s[w - 1] + s[w - 1]) / 3);
  }

  uint8_t *r0 = rowbuf;
  uint8_t *r1 = rowbuf + w;
  uint8_t *r2 = rowbuf + 2 * w;
  memcpy(r0, dst, (size_t)w);
  memcpy(r1, dst, (size_t)w);
  for (int y = 0; y < h; y++) {
    if (y + 1 < h) memcpy(r2, dst + (y + 1) * w, (size_t)w);
    else memcpy(r2, r1, (size_t)w);
    uint8_t *d = dst + y * w;
    for (int x = 0; x < w; x++)
      d[x] = (uint8_t)(((uint32_t)r0[x] + r1[x] + r2[x]) / 3);
    uint8_t *t = r0;
    r0 = r1;
    r1 = r2;
    r2 = t;
  }
  return true;
}

bool ESP32P4_Cv::threshold(const uint8_t *src, int w, int h, uint8_t *dst, uint8_t thr,
                           esp32p4_thresh_t type, const esp32p4_rect_t *roi) {
  if (!src || !dst || w <= 0 || h <= 0) return false;
  int x0, y0, x1, y1;
  clampRoi(w, h, roi, x0, y0, x1, y1);
  const bool cropped = roi != nullptr;
  const int dw = cropped ? (x1 - x0) : w;
  const int dh = cropped ? (y1 - y0) : h;
  for (int y = 0; y < dh; y++) {
    const uint8_t *row = cropped ? (src + (y0 + y) * w + x0) : (src + y * w);
    uint8_t *drow = cropped ? (dst + y * dw) : (dst + y * w);
    for (int x = 0; x < dw; x++) {
      bool on = row[x] > thr;
      if (type == ESP32P4_THRESH_BINARY_INV) on = !on;
      drow[x] = on ? 255 : 0;
    }
  }
  return true;
}

uint8_t ESP32P4_Cv::otsu(const uint8_t *gray, size_t pixels) {
  if (!gray || !pixels) return 128;
  uint32_t hist[256];
  memset(hist, 0, sizeof(hist));
  for (size_t i = 0; i < pixels; i++) hist[gray[i]]++;

  double total = (double)pixels;
  double sum = 0;
  for (int i = 0; i < 256; i++) sum += (double)i * hist[i];

  double sum_b = 0;
  double w_b = 0;
  double max_var = -1;
  int best = 128;
  for (int t = 0; t < 256; t++) {
    w_b += hist[t];
    if (w_b <= 0) continue;
    double w_f = total - w_b;
    if (w_f <= 0) break;
    sum_b += (double)t * hist[t];
    double m_b = sum_b / w_b;
    double m_f = (sum - sum_b) / w_f;
    double diff = m_b - m_f;
    double var = w_b * w_f * diff * diff;
    if (var > max_var) {
      max_var = var;
      best = t;
    }
  }
  return (uint8_t)best;
}

bool ESP32P4_Cv::adaptiveThreshold(const uint8_t *src, int w, int h, uint8_t *dst, int block_size,
                                   int C, bool dark, uint32_t *integral_scratch) {
  if (!src || !dst || w <= 0 || h <= 0) return false;
  if (block_size < 3) block_size = 3;
  if ((block_size & 1) == 0) block_size++;
  const int r = block_size / 2;

  const size_t isize = (size_t)(w + 1) * (size_t)(h + 1);
  bool own = false;
  uint32_t *ii = integral_scratch;
  if (!ii) {
    ii = (uint32_t *)esp32p4_psram_alloc(isize * sizeof(uint32_t));
    if (!ii) return false;
    own = true;
  }
  memset(ii, 0, isize * sizeof(uint32_t));
  for (int y = 0; y < h; y++) {
    uint32_t row = 0;
    for (int x = 0; x < w; x++) {
      row += src[y * w + x];
      ii[(y + 1) * (w + 1) + (x + 1)] = ii[y * (w + 1) + (x + 1)] + row;
    }
  }

  auto rectSum = [&](int x0, int y0, int x1, int y1) -> uint32_t {
    // inclusive coords clamped
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (y1 >= h) y1 = h - 1;
    if (x1 < x0 || y1 < y0) return 0;
    const int A = y0 * (w + 1) + x0;
    const int B = y0 * (w + 1) + (x1 + 1);
    const int Cc = (y1 + 1) * (w + 1) + x0;
    const int D = (y1 + 1) * (w + 1) + (x1 + 1);
    return ii[D] - ii[B] - ii[Cc] + ii[A];
  };

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int x0 = x - r, y0 = y - r, x1 = x + r, y1 = y + r;
      int bw = (x1 < w ? x1 : w - 1) - (x0 > 0 ? x0 : 0) + 1;
      int bh = (y1 < h ? y1 : h - 1) - (y0 > 0 ? y0 : 0) + 1;
      uint32_t sum = rectSum(x0, y0, x1, y1);
      int mean = (int)(sum / (uint32_t)(bw * bh));
      int pix = src[y * w + x];
      bool on = dark ? (pix < mean - C) : (pix > mean + C);
      dst[y * w + x] = on ? 255 : 0;
    }
  }
  if (own) esp32p4_psram_free(ii);
  return true;
}

bool ESP32P4_Cv::morphologyOpen(const uint8_t *src, int w, int h, uint8_t *dst, int iterations) {
  if (!src || !dst || iterations < 1) return false;
  static uint8_t *tmp = nullptr;
  static size_t cap = 0;
  size_t n = (size_t)w * (size_t)h;
  if (cap < n) {
    esp32p4_psram_free(tmp);
    tmp = (uint8_t *)esp32p4_psram_alloc(n);
    cap = tmp ? n : 0;
    if (!tmp) return false;
  }
  return erode(src, w, h, tmp, iterations) && dilate(tmp, w, h, dst, iterations);
}

bool ESP32P4_Cv::morphologyClose(const uint8_t *src, int w, int h, uint8_t *dst, int iterations) {
  if (!src || !dst || iterations < 1) return false;
  static uint8_t *tmp = nullptr;
  static size_t cap = 0;
  size_t n = (size_t)w * (size_t)h;
  if (cap < n) {
    esp32p4_psram_free(tmp);
    tmp = (uint8_t *)esp32p4_psram_alloc(n);
    cap = tmp ? n : 0;
    if (!tmp) return false;
  }
  return dilate(src, w, h, tmp, iterations) && erode(tmp, w, h, dst, iterations);
}

esp32p4_hsv_t ESP32P4_Cv::rgb565ToHsv(uint16_t px) {
  int r = ((px >> 11) & 0x1F) * 255 / 31;
  int g = ((px >> 5) & 0x3F) * 255 / 63;
  int b = (px & 0x1F) * 255 / 31;
  int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
  int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
  int v = mx;
  int delta = mx - mn;
  int s = (mx == 0) ? 0 : (delta * 255 / mx);
  int h = 0;
  if (delta != 0) {
    if (mx == r) h = (60 * (g - b) / delta + 360) % 360;
    else if (mx == g) h = 60 * (b - r) / delta + 120;
    else h = 60 * (r - g) / delta + 240;
  }
  esp32p4_hsv_t o;
  o.h = (uint8_t)(h / 2);  // 0..179
  o.s = (uint8_t)s;
  o.v = (uint8_t)v;
  return o;
}

static bool hsvInRange(esp32p4_hsv_t v, esp32p4_hsv_t lo, esp32p4_hsv_t hi) {
  if (v.s < lo.s || v.s > hi.s || v.v < lo.v || v.v > hi.v) return false;
  if (lo.h <= hi.h) return v.h >= lo.h && v.h <= hi.h;
  return v.h >= lo.h || v.h <= hi.h;  // wrap
}

bool ESP32P4_Cv::inRangeHsv(const uint16_t *src, int w, int h, uint8_t *dst, esp32p4_hsv_t lo,
                            esp32p4_hsv_t hi, const esp32p4_rect_t *roi) {
  if (!src || !dst || w <= 0 || h <= 0) return false;
  int x0, y0, x1, y1;
  clampRoi(w, h, roi, x0, y0, x1, y1);
  const bool cropped = roi != nullptr;
  const int dw = cropped ? (x1 - x0) : w;
  const int dh = cropped ? (y1 - y0) : h;
  for (int y = 0; y < dh; y++) {
    const uint16_t *row = cropped ? (src + (y0 + y) * w + x0) : (src + y * w);
    uint8_t *drow = cropped ? (dst + y * dw) : (dst + y * w);
    for (int x = 0; x < dw; x++) drow[x] = hsvInRange(rgb565ToHsv(row[x]), lo, hi) ? 255 : 0;
  }
  return true;
}

static bool morphEnsure(size_t n, uint8_t **a, uint8_t **b) {
  static uint8_t *sa = nullptr, *sb = nullptr;
  static size_t cap = 0;
  if (cap < n) {
    esp32p4_psram_free(sa);
    esp32p4_psram_free(sb);
    sa = (uint8_t *)esp32p4_psram_alloc(n);
    sb = (uint8_t *)esp32p4_psram_alloc(n);
    cap = (sa && sb) ? n : 0;
    if (!cap) {
      esp32p4_psram_free(sa);
      esp32p4_psram_free(sb);
      sa = sb = nullptr;
      return false;
    }
  }
  *a = sa;
  *b = sb;
  return true;
}

static bool morphOp(const uint8_t *src, int w, int h, uint8_t *dst, bool dilate_op, int iterations,
                    const esp32p4_rect_t *roi) {
  if (!src || !dst || w < 3 || h < 3 || iterations < 1) return false;
  int x0, y0, x1, y1;
  clampRoi(w, h, roi, x0, y0, x1, y1);
  const bool cropped = roi != nullptr;
  const int dw = cropped ? (x1 - x0) : w;
  const int dh = cropped ? (y1 - y0) : h;
  const size_t n = (size_t)dw * (size_t)dh;

  uint8_t *a = nullptr, *b = nullptr;
  if (!morphEnsure(n, &a, &b)) return false;
  if (cropped) {
    for (int y = 0; y < dh; y++) memcpy(a + y * dw, src + (y0 + y) * w + x0, (size_t)dw);
  } else {
    memcpy(a, src, n);
  }

  uint8_t *cur = a;
  uint8_t *nxt = b;
  for (int it = 0; it < iterations; it++) {
    // Interior: no clamps (OpenCV-style speed path)
    for (int y = 1; y < dh - 1; y++) {
      const uint8_t *r0 = cur + (y - 1) * dw;
      const uint8_t *r1 = cur + y * dw;
      const uint8_t *r2 = cur + (y + 1) * dw;
      uint8_t *d = nxt + y * dw;
      for (int x = 1; x < dw - 1; x++) {
        if (dilate_op) {
          uint8_t m = r0[x - 1] | r0[x] | r0[x + 1] | r1[x - 1] | r1[x] | r1[x + 1] | r2[x - 1] |
                      r2[x] | r2[x + 1];
          d[x] = m ? 255 : 0;
        } else {
          uint8_t m = r0[x - 1] & r0[x] & r0[x + 1] & r1[x - 1] & r1[x] & r1[x + 1] & r2[x - 1] &
                      r2[x] & r2[x + 1];
          d[x] = m ? 255 : 0;
        }
      }
      // left/right border of row
      d[0] = dilate_op ? (r1[0] | r1[1] ? 255 : 0) : (r1[0] & r1[1] ? 255 : 0);
      d[dw - 1] = dilate_op ? (r1[dw - 2] | r1[dw - 1] ? 255 : 0)
                            : (r1[dw - 2] & r1[dw - 1] ? 255 : 0);
    }
    // top/bottom rows (copy nearest interior treatment simply)
    memcpy(nxt, nxt + dw, (size_t)dw);
    memcpy(nxt + (dh - 1) * dw, nxt + (dh - 2) * dw, (size_t)dw);
    uint8_t *t = cur;
    cur = nxt;
    nxt = t;
  }
  memcpy(dst, cur, n);
  return true;
}

bool ESP32P4_Cv::erode(const uint8_t *src, int w, int h, uint8_t *dst, int iterations,
                       const esp32p4_rect_t *roi) {
  return morphOp(src, w, h, dst, false, iterations, roi);
}

bool ESP32P4_Cv::dilate(const uint8_t *src, int w, int h, uint8_t *dst, int iterations,
                        const esp32p4_rect_t *roi) {
  return morphOp(src, w, h, dst, true, iterations, roi);
}

bool ESP32P4_Cv::edges(const uint8_t *src, int w, int h, uint8_t *dst, uint8_t thr_lo,
                       uint8_t thr_hi, uint16_t *mag_scratch, const esp32p4_rect_t *roi) {
  if (!src || !dst || w < 3 || h < 3) return false;
  if (roi) {
    // Rare path — keep compatible
    int x0, y0, x1, y1;
    clampRoi(w, h, roi, x0, y0, x1, y1);
    const int dw = x1 - x0, dh = y1 - y0;
    uint16_t *mag = mag_scratch;
    bool own = false;
    if (!mag) {
      mag = (uint16_t *)esp32p4_psram_alloc((size_t)dw * (size_t)dh * sizeof(uint16_t));
      if (!mag) return false;
      own = true;
    }
    for (int y = 0; y < dh; y++) {
      for (int x = 0; x < dw; x++) {
        auto gAt = [&](int xx, int yy) -> int {
          if (xx < 0) xx = 0;
          if (yy < 0) yy = 0;
          if (xx >= dw) xx = dw - 1;
          if (yy >= dh) yy = dh - 1;
          return src[(y0 + yy) * w + (x0 + xx)];
        };
        int gx = -gAt(x - 1, y - 1) - 2 * gAt(x - 1, y) - gAt(x - 1, y + 1) + gAt(x + 1, y - 1) +
                 2 * gAt(x + 1, y) + gAt(x + 1, y + 1);
        int gy = -gAt(x - 1, y - 1) - 2 * gAt(x, y - 1) - gAt(x + 1, y - 1) + gAt(x - 1, y + 1) +
                 2 * gAt(x, y + 1) + gAt(x + 1, y + 1);
        if (gx < 0) gx = -gx;
        if (gy < 0) gy = -gy;
        uint32_t m = (uint32_t)gx + (uint32_t)gy;
        if (m > 65535) m = 65535;
        mag[y * dw + x] = (uint16_t)m;
      }
    }
    for (int y = 0; y < dh; y++) {
      uint8_t *drow = dst + y * dw;
      for (int x = 0; x < dw; x++) {
        uint16_t m = mag[y * dw + x];
        drow[x] = (m >= thr_hi) ? 255 : (m >= thr_lo ? 128 : 0);
      }
    }
    if (own) esp32p4_psram_free(mag);
    return true;
  }

  uint16_t *mag = mag_scratch;
  bool own = false;
  if (!mag) {
    mag = (uint16_t *)esp32p4_psram_alloc((size_t)w * (size_t)h * sizeof(uint16_t));
    if (!mag) return false;
    own = true;
  }

  // Fast Sobel on interior with row pointers (no per-pixel clamps).
  memset(mag, 0, (size_t)w * sizeof(uint16_t));
  memset(mag + (h - 1) * w, 0, (size_t)w * sizeof(uint16_t));
  for (int y = 1; y < h - 1; y++) {
    const uint8_t *r0 = src + (y - 1) * w;
    const uint8_t *r1 = src + y * w;
    const uint8_t *r2 = src + (y + 1) * w;
    uint16_t *mrow = mag + y * w;
    mrow[0] = 0;
    mrow[w - 1] = 0;
    for (int x = 1; x < w - 1; x++) {
      int gx = -r0[x - 1] - 2 * r1[x - 1] - r2[x - 1] + r0[x + 1] + 2 * r1[x + 1] + r2[x + 1];
      int gy = -r0[x - 1] - 2 * r0[x] - r0[x + 1] + r2[x - 1] + 2 * r2[x] + r2[x + 1];
      if (gx < 0) gx = -gx;
      if (gy < 0) gy = -gy;
      uint32_t m = (uint32_t)gx + (uint32_t)gy;
      if (m > 65535) m = 65535;
      mrow[x] = (uint16_t)m;
    }
  }

  for (int y = 0; y < h; y++) {
    uint8_t *drow = dst + y * w;
    const uint16_t *mrow = mag + y * w;
    for (int x = 0; x < w; x++) {
      uint16_t m = mrow[x];
      drow[x] = (m >= thr_hi) ? 255 : (m >= thr_lo ? 128 : 0);
    }
  }
  if (own) esp32p4_psram_free(mag);
  return true;
}

int ESP32P4_Cv::findBlobs(const uint8_t *bin, int w, int h, esp32p4_blob_t *out, int max_out,
                          int min_area, uint16_t *label_scratch, const esp32p4_rect_t *roi) {
  if (!bin || !out || !label_scratch || max_out <= 0 || w <= 0 || h <= 0) return 0;
  int x0, y0, x1, y1;
  clampRoi(w, h, roi, x0, y0, x1, y1);
  const bool cropped = roi != nullptr;
  const int dw = cropped ? (x1 - x0) : w;
  const int dh = cropped ? (y1 - y0) : h;
  memset(label_scratch, 0, (size_t)dw * (size_t)dh * sizeof(uint16_t));

  // Keep union-find / blob stats off the caller's stack (MJPEG worker ~12–16KB).
  const int MAX_LABELS = 256;
  struct BlobWork {
    uint16_t parent[256];
    int area[256];
    int minx[256], miny[256], maxx[256], maxy[256];
    int32_t sx[256], sy[256];
  };
  static BlobWork *bw = nullptr;
  if (!bw) {
    bw = (BlobWork *)esp32p4_psram_alloc(sizeof(BlobWork));
    if (!bw) return 0;
  }

  for (int i = 0; i < MAX_LABELS; i++) {
    bw->parent[i] = (uint16_t)i;
    bw->area[i] = 0;
    bw->minx[i] = dw;
    bw->miny[i] = dh;
    bw->maxx[i] = 0;
    bw->maxy[i] = 0;
    bw->sx[i] = 0;
    bw->sy[i] = 0;
  }

  auto find = [&](uint16_t a) -> uint16_t {
    while (bw->parent[a] != a) a = bw->parent[a] = bw->parent[bw->parent[a]];
    return a;
  };
  auto unite = [&](uint16_t a, uint16_t b) {
    a = find(a);
    b = find(b);
    if (a != b) bw->parent[b] = a;
  };

  uint16_t next = 1;
  auto pix = [&](int x, int y) -> uint8_t {
    return cropped ? bin[(y0 + y) * w + (x0 + x)] : bin[y * w + x];
  };

  for (int y = 0; y < dh; y++) {
    for (int x = 0; x < dw; x++) {
      if (!pix(x, y)) continue;
      uint16_t left = (x > 0) ? label_scratch[y * dw + (x - 1)] : 0;
      uint16_t up = (y > 0) ? label_scratch[(y - 1) * dw + x] : 0;
      if (!left && !up) {
        if (next >= MAX_LABELS) continue;
        label_scratch[y * dw + x] = next++;
      } else if (left && !up) {
        label_scratch[y * dw + x] = left;
      } else if (!left && up) {
        label_scratch[y * dw + x] = up;
      } else {
        label_scratch[y * dw + x] = left;
        unite(left, up);
      }
    }
  }

  for (int y = 0; y < dh; y++) {
    for (int x = 0; x < dw; x++) {
      uint16_t lab = label_scratch[y * dw + x];
      if (!lab) continue;
      lab = find(lab);
      label_scratch[y * dw + x] = lab;
      bw->area[lab]++;
      bw->sx[lab] += x;
      bw->sy[lab] += y;
      if (x < bw->minx[lab]) bw->minx[lab] = x;
      if (y < bw->miny[lab]) bw->miny[lab] = y;
      if (x > bw->maxx[lab]) bw->maxx[lab] = x;
      if (y > bw->maxy[lab]) bw->maxy[lab] = y;
    }
  }

  int count = 0;
  for (uint16_t lab = 1; lab < next && count < max_out; lab++) {
    if (find(lab) != lab) continue;
    if (bw->area[lab] < min_area) continue;
    esp32p4_blob_t &b = out[count++];
    b.area = bw->area[lab];
    b.cx = (int)(bw->sx[lab] / bw->area[lab]) + (cropped ? x0 : 0);
    b.cy = (int)(bw->sy[lab] / bw->area[lab]) + (cropped ? y0 : 0);
    b.box.x = bw->minx[lab] + (cropped ? x0 : 0);
    b.box.y = bw->miny[lab] + (cropped ? y0 : 0);
    b.box.w = bw->maxx[lab] - bw->minx[lab] + 1;
    b.box.h = bw->maxy[lab] - bw->miny[lab] + 1;
  }
  return count;
}

static void putPx(uint16_t *img, int w, int h, int x, int y, uint16_t color) {
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  img[y * w + x] = color;
}

void ESP32P4_Cv::line(uint16_t *img, int w, int h, int x0, int y0, int x1, int y1, uint16_t color,
                      int thickness) {
  if (!img) return;
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    for (int t = -thickness / 2; t <= thickness / 2; t++) {
      putPx(img, w, h, x0 + t, y0, color);
      putPx(img, w, h, x0, y0 + t, color);
    }
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void ESP32P4_Cv::circle(uint16_t *img, int w, int h, int cx, int cy, int radius, uint16_t color,
                        int thickness) {
  if (!img || radius <= 0) return;
  for (int t = 0; t < thickness; t++) {
    int r = radius - t;
    if (r <= 0) break;
    int x = r, y = 0, err = 0;
    while (x >= y) {
      putPx(img, w, h, cx + x, cy + y, color);
      putPx(img, w, h, cx + y, cy + x, color);
      putPx(img, w, h, cx - y, cy + x, color);
      putPx(img, w, h, cx - x, cy + y, color);
      putPx(img, w, h, cx - x, cy - y, color);
      putPx(img, w, h, cx - y, cy - x, color);
      putPx(img, w, h, cx + y, cy - x, color);
      putPx(img, w, h, cx + x, cy - y, color);
      y++;
      err += 1 + 2 * y;
      if (2 * (err - x) + 1 > 0) {
        x--;
        err += 1 - 2 * x;
      }
    }
  }
}

// Tiny 5x7 font (ASCII 32..90 subset for digits/letters)
static const uint8_t FONT5X7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  // space
    {0x00, 0x00, 0x5F, 0x00, 0x00},  // !
    {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
    {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
    {0x27, 0x45, 0x45, 0x45, 0x39},  // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
    {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
    {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
};

static int fontIndex(char c) {
  if (c == ' ') return 0;
  if (c == '!') return 1;
  if (c >= '0' && c <= '9') return 2 + (c - '0');
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  // Map A-Z to a simple pattern via digits fallback for unknown → space
  if (c >= 'A' && c <= 'Z') {
    // Minimal letter set: use bit patterns derived from ASCII index mod — keep readable digits mostly
    static const uint8_t LETTERS[26][5] = {
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
        {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
        {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
        {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
        {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
    };
    (void)LETTERS;
    return -100 - (c - 'A');  // special
  }
  return 0;
}

void ESP32P4_Cv::putText(uint16_t *img, int w, int h, int x, int y, const char *text,
                         uint16_t color, int scale) {
  if (!img || !text || scale < 1) return;
  static const uint8_t LETTERS[26][5] = {
      {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
      {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
      {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
      {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
      {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
      {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
      {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
      {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
      {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
  };
  int cx = x;
  for (const char *p = text; *p; p++) {
    char c = *p;
    const uint8_t *glyph = FONT5X7[0];
    int idx = fontIndex(c);
    if (idx >= 0 && idx < (int)(sizeof(FONT5X7) / sizeof(FONT5X7[0]))) glyph = FONT5X7[idx];
    else if (idx <= -100) glyph = LETTERS[(-100 - idx)];
    for (int col = 0; col < 5; col++) {
      uint8_t bits = glyph[col];
      for (int row = 0; row < 7; row++) {
        if (bits & (1 << row)) {
          for (int sy = 0; sy < scale; sy++)
            for (int sx = 0; sx < scale; sx++)
              putPx(img, w, h, cx + col * scale + sx, y + row * scale + sy, color);
        }
      }
    }
    cx += 6 * scale;
  }
}

void ESP32P4_Cv::drawBlob(uint16_t *img, int w, int h, const esp32p4_blob_t &b, uint16_t color,
                          int thickness) {
  ESP32P4_Img::fillRect565(img, w, h, b.box, color, thickness);
  circle(img, w, h, b.cx, b.cy, 4, color, 2);
}
