#pragma once

/**
 * ESP-VISION–inspired AI helpers for ESP32CSI_Vision.
 *
 * Provides detection result types, letterbox preprocess (RGB565 → model RGB888),
 * box remapping, and NMS — the same pipeline shape as ESP-VISION's espdl wrappers,
 * without bundling model weights (load .espdl yourself under ESP-IDF / ESP-DL).
 *
 * For face detect on IDF, see idf_src/ESP32P4_FaceDetect.h.
 */

#include <stddef.h>
#include <stdint.h>

#include "img/ESP32P4_Img.h"

struct esp32p4_det_t {
  float score;         // 0..1
  int x, y, w, h;      // axis-aligned box in source pixels (top-left + size)
  int category;        // class id (COCO id, or 0 for single-class models)
  const char *label;   // class name ("dog", "person", …); static string, never free
};

struct esp32p4_keypoint_t {
  float x, y;
  float score;  // 0 if missing
};

struct esp32p4_pose_t {
  esp32p4_det_t box;
  esp32p4_keypoint_t kp[17];  // COCO-17 (ESP-VISION YOLO11nPose)
};

struct esp32p4_seg_t {
  esp32p4_det_t box;
  const uint8_t *mask;  // box-sized 0/1 raster, owned by ESP32P4_Seg until next detect()
  int mask_w;
  int mask_h;
};

struct esp32p4_cls_t {
  const char *label;
  float score;
};

struct esp32p4_gesture_t {
  esp32p4_det_t hand;
  const char *label;
  float score;
};

struct esp32p4_reid_t {
  esp32p4_det_t box;
  int id;  // enrolled id, or -1 if unknown
  float similarity;
};

struct esp32p4_ocr_t {
  int points[8];  // x0,y0 … x3,y3
  float score;
  char text[96];
};

struct esp32p4_letterbox_t {
  float scale;
  int pad_x;
  int pad_y;
  int model_w;
  int model_h;
  int src_w;
  int src_h;
};

class ESP32P4_VisionAi {
 public:
  /**
   * Letterbox RGB565 → packed RGB888 (HWC) for ESP-DL / TFLite inputs.
   * dst must hold model_w * model_h * 3 bytes (PSRAM recommended).
   * pad_rgb888: fill color for bars (default gray 114 like ESP-VISION).
   */
  static bool letterboxRgb565(const uint16_t *src, int src_w, int src_h, uint8_t *dst, int model_w,
                              int model_h, esp32p4_letterbox_t *meta, uint8_t pad_r = 114,
                              uint8_t pad_g = 114, uint8_t pad_b = 114);

  /** Map a box from model space back to source pixels. */
  static void mapBoxToSrc(const esp32p4_letterbox_t &lb, float mx, float my, float mw, float mh,
                          esp32p4_det_t *out);

  /** Map a keypoint from model space to source pixels. */
  static void mapPointToSrc(const esp32p4_letterbox_t &lb, float mx, float my, float *sx, float *sy);

  /**
   * Greedy NMS on axis-aligned boxes (score descending assumed or sorted here).
   * Returns kept count; writes compacted indices into keep_idx.
   */
  static int nms(const esp32p4_det_t *dets, int n, float iou_thr, int *keep_idx, int max_keep);

  static float iou(const esp32p4_det_t &a, const esp32p4_det_t &b);

  /** Softmax in-place on float logits (classification heads). */
  static void softmax(float *logits, int n);

  /** Draw detection boxes + optional label onto RGB565. */
  static void drawDets(uint16_t *img, int w, int h, const esp32p4_det_t *dets, int n,
                       uint16_t color, int thickness = 2);

  /**
   * JSON for other projects / HTTP / Serial.
   * {"n":2,"ms":35,"dets":[{"label":"dog","class":16,"score":0.87,"x":10,"y":20,"w":100,"h":80}]}
   * Returns bytes written (excluding NUL), or 0 on failure.
   */
  static size_t detsToJson(const esp32p4_det_t *dets, int n, char *buf, size_t cap, int ms = -1);
  /** Human line: "cat 0.87 @ 10,20 100x80 · cat 0.40 @ …" */
  static size_t detsToLine(const esp32p4_det_t *dets, int n, char *buf, size_t cap);
  static size_t clsToJson(const esp32p4_cls_t *hits, int n, char *buf, size_t cap, int ms = -1);
  static size_t ocrToJson(const esp32p4_ocr_t *hits, int n, char *buf, size_t cap, int ms = -1);
  static size_t gestureToJson(const esp32p4_gesture_t *hits, int n, char *buf, size_t cap,
                              int ms = -1);
};
