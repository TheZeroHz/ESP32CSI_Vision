#pragma once

/**
 * Lean OpenCV-compatible core types for ESP32-P4 (no full OpenCV dependency).
 * Geometry + Mat depth/channel encoding used by src/opencv/*.
 */

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "img/ESP32P4_Img.h"

namespace esp_cv {

enum Depth : int {
  CV_8U = 0,
  CV_8S = 1,
  CV_16U = 2,
  CV_16S = 3,
  CV_32S = 4,
  CV_32F = 5,
  CV_64F = 6,
};

inline constexpr int makeType(int depth, int cn) {
  return (depth & 7) + (((cn)-1) << 3);
}

inline constexpr int CV_8UC1 = makeType(CV_8U, 1);
inline constexpr int CV_8UC2 = makeType(CV_8U, 2);  // RGB565 packed as 2 bytes/px
inline constexpr int CV_8UC3 = makeType(CV_8U, 3);  // RGB888 / BGR888
inline constexpr int CV_8UC4 = makeType(CV_8U, 4);
inline constexpr int CV_16UC1 = makeType(CV_16U, 1);
inline constexpr int CV_16SC1 = makeType(CV_16S, 1);
inline constexpr int CV_32FC1 = makeType(CV_32F, 1);
inline constexpr int CV_32FC2 = makeType(CV_32F, 2);
inline constexpr int CV_32FC3 = makeType(CV_32F, 3);

inline int depth(int type) { return type & 7; }
inline int channels(int type) { return (type >> 3) + 1; }

inline size_t elemSize1(int type) {
  static const size_t kTab[] = {1, 1, 2, 2, 4, 4, 8};
  int d = depth(type);
  if (d < 0 || d > 6) return 1;
  return kTab[d];
}

inline size_t elemSize(int type) { return elemSize1(type) * (size_t)channels(type); }

struct Point {
  int x = 0;
  int y = 0;
  Point() = default;
  Point(int x_, int y_) : x(x_), y(y_) {}
};

struct Point2f {
  float x = 0.f;
  float y = 0.f;
  Point2f() = default;
  Point2f(float x_, float y_) : x(x_), y(y_) {}
  explicit Point2f(const Point &p) : x((float)p.x), y((float)p.y) {}
};

struct Size {
  int width = 0;
  int height = 0;
  Size() = default;
  Size(int w, int h) : width(w), height(h) {}
  int area() const { return width * height; }
  bool empty() const { return width <= 0 || height <= 0; }
};

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  Rect() = default;
  Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), width(w_), height(h_) {}
  Rect(const Point &org, const Size &sz) : x(org.x), y(org.y), width(sz.width), height(sz.height) {}
  explicit Rect(const esp32p4_rect_t &r) : x(r.x), y(r.y), width(r.w), height(r.h) {}
  esp32p4_rect_t toEsp() const { return esp32p4_rect_t{x, y, width, height}; }
  Size size() const { return Size(width, height); }
  Point tl() const { return Point(x, y); }
  Point br() const { return Point(x + width, y + height); }
  int area() const { return width * height; }
  bool empty() const { return width <= 0 || height <= 0; }
  bool contains(const Point &p) const {
    return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height;
  }
};

struct Scalar {
  double v[4] = {0, 0, 0, 0};
  Scalar() = default;
  explicit Scalar(double v0) {
    v[0] = v0;
    v[1] = v[2] = v[3] = 0;
  }
  Scalar(double v0, double v1, double v2 = 0, double v3 = 0) {
    v[0] = v0;
    v[1] = v1;
    v[2] = v2;
    v[3] = v3;
  }
  double operator[](int i) const { return (i >= 0 && i < 4) ? v[i] : 0; }
  double &operator[](int i) { return v[(i >= 0 && i < 4) ? i : 0]; }
};

struct Range {
  int start = 0;
  int end = 0;
  Range() = default;
  Range(int s, int e) : start(s), end(e) {}
  static Range all() { return Range(INT32_MIN, INT32_MAX); }
  int size() const { return end > start ? (end - start) : 0; }
  bool empty() const { return end <= start; }
};

/** Basic buffer / geometry validation for CV entry points. */
inline bool validImageSize(int w, int h) { return w > 0 && h > 0 && w <= 8192 && h <= 8192; }

inline bool validType(int type) {
  int d = depth(type);
  int c = channels(type);
  return d >= 0 && d <= 6 && c >= 1 && c <= 4;
}

}  // namespace esp_cv
