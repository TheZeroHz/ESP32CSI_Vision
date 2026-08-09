#pragma once

/**
 * OpenCV-compatible Mat for ESP32-P4:
 *  - PSRAM-backed create()
 *  - external buffer wrap (zero-copy camera FB / scratch)
 *  - step / stride
 *  - ROI / submatrix views (non-owning)
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mem/ESP32P4_Psram.h"
#include "opencv/esp_cv_types.h"

namespace esp_cv {

#ifndef ESP_CV_AUTO_STEP
#define ESP_CV_AUTO_STEP ((size_t)0)
#endif

class Mat {
 public:
  Mat() = default;
  ~Mat() { release(); }

  Mat(const Mat &other) { *this = other; }
  Mat &operator=(const Mat &other) {
    if (this == &other) return *this;
    release();
    rows = other.rows;
    cols = other.cols;
    type_ = other.type_;
    step = other.step;
    data = other.data;
    owns_ = false;  // shallow copy — use clone() for deep
    return *this;
  }

  Mat(Mat &&other) noexcept { moveFrom(other); }
  Mat &operator=(Mat &&other) noexcept {
    if (this == &other) return *this;
    release();
    moveFrom(other);
    return *this;
  }

  /** Allocate continuous Mat in PSRAM (cache-aligned). */
  Mat(int rows_, int cols_, int type_) { create(rows_, cols_, type_); }

  /**
   * Wrap external buffer (camera FB, stack/PSRAM scratch). Does not free data.
   * step=ESP_CV_AUTO_STEP → cols * elemSize().
   */
  Mat(int rows_, int cols_, int type_, void *data_, size_t step_ = ESP_CV_AUTO_STEP) {
    createHeader(rows_, cols_, type_, data_, step_, false);
  }

  static Mat zeros(int rows, int cols, int type) {
    Mat m;
    if (m.create(rows, cols, type)) memset(m.data, 0, m.rows * m.step);
    return m;
  }

  bool create(int rows_, int cols_, int type_) {
    release();
    if (!validImageSize(cols_, rows_) || !validType(type_)) return false;
    size_t es = ::esp_cv::elemSize(type_);
    size_t step_ = (size_t)cols_ * es;
    size_t bytes = step_ * (size_t)rows_;
    void *p = esp32p4_psram_alloc(bytes);
    if (!p) return false;
    createHeader(rows_, cols_, type_, p, step_, true);
    return true;
  }

  void release() {
    if (owns_ && data) esp32p4_psram_free(data);
    data = nullptr;
    rows = cols = 0;
    type_ = 0;
    step = 0;
    owns_ = false;
  }

  bool empty() const { return !data || rows <= 0 || cols <= 0; }
  int type() const { return type_; }
  int depth() const { return esp_cv::depth(type_); }
  int channels() const { return esp_cv::channels(type_); }
  size_t elemSize() const { return esp_cv::elemSize(type_); }
  size_t elemSize1() const { return esp_cv::elemSize1(type_); }
  Size size() const { return Size(cols, rows); }
  size_t total() const { return (size_t)rows * (size_t)cols; }
  bool isContinuous() const { return step == (size_t)cols * elemSize(); }

  uint8_t *ptr(int y = 0) {
    if (!data || y < 0 || y >= rows) return nullptr;
    return data + (size_t)y * step;
  }
  const uint8_t *ptr(int y = 0) const {
    if (!data || y < 0 || y >= rows) return nullptr;
    return data + (size_t)y * step;
  }

  template <typename T>
  T *ptr(int y = 0) {
    return reinterpret_cast<T *>(ptr(y));
  }
  template <typename T>
  const T *ptr(int y = 0) const {
    return reinterpret_cast<const T *>(ptr(y));
  }

  /** ROI / submatrix view — does not own memory; parent Mat must outlive view. */
  Mat operator()(const Rect &roi) const {
    Mat view;
    if (empty() || roi.empty()) return view;
    Rect r = roi;
    if (r.x < 0) {
      r.width += r.x;
      r.x = 0;
    }
    if (r.y < 0) {
      r.height += r.y;
      r.y = 0;
    }
    if (r.x + r.width > cols) r.width = cols - r.x;
    if (r.y + r.height > rows) r.height = rows - r.y;
    if (r.width <= 0 || r.height <= 0) return view;
    uint8_t *p = const_cast<uint8_t *>(ptr(r.y));
    if (!p) return view;
    p += (size_t)r.x * elemSize();
    view.createHeader(r.height, r.width, type_, p, step, false);
    return view;
  }

  Mat rowRange(const Range &r) const {
    int s = r.start == INT32_MIN ? 0 : r.start;
    int e = r.end == INT32_MAX ? rows : r.end;
    if (s < 0) s = 0;
    if (e > rows) e = rows;
    if (e <= s) return Mat();
    return (*this)(Rect(0, s, cols, e - s));
  }

  Mat colRange(const Range &r) const {
    int s = r.start == INT32_MIN ? 0 : r.start;
    int e = r.end == INT32_MAX ? cols : r.end;
    if (s < 0) s = 0;
    if (e > cols) e = cols;
    if (e <= s) return Mat();
    return (*this)(Rect(s, 0, e - s, rows));
  }

  /** Deep copy into PSRAM-owned Mat. */
  Mat clone() const {
    Mat out;
    if (empty()) return out;
    if (!out.create(rows, cols, type_)) return out;
    if (isContinuous() && out.isContinuous()) {
      memcpy(out.data, data, (size_t)rows * step);
    } else {
      size_t rowb = (size_t)cols * elemSize();
      for (int y = 0; y < rows; ++y) memcpy(out.ptr(y), ptr(y), rowb);
    }
    return out;
  }

  bool copyTo(Mat &dst) const {
    if (empty()) {
      dst.release();
      return true;
    }
    if (dst.rows != rows || dst.cols != cols || dst.type_ != type_ || !dst.data) {
      if (!dst.create(rows, cols, type_)) return false;
    }
    size_t rowb = (size_t)cols * elemSize();
    for (int y = 0; y < rows; ++y) {
      const uint8_t *s = ptr(y);
      uint8_t *d = dst.ptr(y);
      if (!s || !d) return false;
      memcpy(d, s, rowb);
    }
    return true;
  }

  /** Fill with scalar (first channel for gray; up to 4 for multi-channel 8U). */
  bool setTo(const Scalar &s) {
    if (empty()) return false;
    int cn = channels();
    if (depth() != CV_8U) return false;
    uint8_t pix[4];
    for (int i = 0; i < 4; ++i) {
      double t = s[i];
      if (t < 0) t = 0;
      if (t > 255) t = 255;
      pix[i] = (uint8_t)(t + 0.5);
    }
    size_t es = elemSize();
    for (int y = 0; y < rows; ++y) {
      uint8_t *row = ptr(y);
      if (!row) return false;
      for (int x = 0; x < cols; ++x) {
        uint8_t *p = row + (size_t)x * es;
        for (int c = 0; c < cn; ++c) p[c] = pix[c];
      }
    }
    return true;
  }

  // Public header fields (OpenCV-style).
  int rows = 0;
  int cols = 0;
  int type_ = 0;
  size_t step = 0;
  uint8_t *data = nullptr;

 private:
  bool owns_ = false;

  void createHeader(int rows_, int cols_, int type__, void *data_, size_t step_, bool owns) {
    rows = rows_;
    cols = cols_;
    type_ = type__;
    data = (uint8_t *)data_;
    owns_ = owns;
    size_t es = esp_cv::elemSize(type__);
    step = (step_ == ESP_CV_AUTO_STEP || step_ == 0) ? ((size_t)cols_ * es) : step_;
  }

  void moveFrom(Mat &other) {
    rows = other.rows;
    cols = other.cols;
    type_ = other.type_;
    step = other.step;
    data = other.data;
    owns_ = other.owns_;
    other.data = nullptr;
    other.owns_ = false;
    other.rows = other.cols = 0;
    other.step = 0;
  }
};

/** Wrap camera RGB565 framebuffer without copy. */
inline Mat wrapRgb565(uint16_t *buf, int w, int h, size_t step_bytes = ESP_CV_AUTO_STEP) {
  return Mat(h, w, CV_8UC2, buf, step_bytes);
}

inline Mat wrapGray(uint8_t *buf, int w, int h, size_t step_bytes = ESP_CV_AUTO_STEP) {
  return Mat(h, w, CV_8UC1, buf, step_bytes);
}

inline Mat wrapRgb888(uint8_t *buf, int w, int h, size_t step_bytes = ESP_CV_AUTO_STEP) {
  return Mat(h, w, CV_8UC3, buf, step_bytes);
}

}  // namespace esp_cv
