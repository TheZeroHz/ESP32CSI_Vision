#pragma once

/**
 * ESP-DL object detect — pass an image, get boxes + class names.
 *
 * Typical merge into another project:
 *   ESP32P4_ObjectDetect det;
 *   det.begin(ESP32P4_DET_DOG_224);
 *   int n = det.infer(fb);                    // or detect(rgb565, w, h, out, max)
 *   const esp32p4_det_t *d = det.results();
 *   // d[i].label, .score, .x .y .w .h, .category
 *   det.resultsJson(json, sizeof(json));
 *
 * Models load from any mounted volume: {/sdcard|/ffat|/littlefs|/spiffs}/models/p4/*.espdl
 * SD is optional — FFat / LittleFS / SPIFFS work without a card.
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "vision/ESP32P4_VisionAi.h"

enum esp32p4_det_model_t : int {
  ESP32P4_DET_COCO_YOLO11N = 0,
  ESP32P4_DET_COCO_YOLO11N_320 = 1,
  ESP32P4_DET_PEDESTRIAN_PICO = 2,
  ESP32P4_DET_CAT_224 = 3,
  ESP32P4_DET_CAT_416 = 4,
  ESP32P4_DET_DOG_224 = 5,
  ESP32P4_DET_DOG_416 = 6,
  ESP32P4_DET_HAND_224 = 7,
  ESP32P4_DET_YOLO26_640 = 8,
  ESP32P4_DET_YOLO26_512 = 9,
};

class ESP32P4_Jpeg;

class ESP32P4_ObjectDetect {
 public:
  enum Model : int {
    COCO_YOLO11N = ESP32P4_DET_COCO_YOLO11N,
    COCO_YOLO11N_320 = ESP32P4_DET_COCO_YOLO11N_320,
    PEDESTRIAN_PICO = ESP32P4_DET_PEDESTRIAN_PICO,
    CAT_224 = ESP32P4_DET_CAT_224,
    CAT_416 = ESP32P4_DET_CAT_416,
    DOG_224 = ESP32P4_DET_DOG_224,
    DOG_416 = ESP32P4_DET_DOG_416,
    HAND_224 = ESP32P4_DET_HAND_224,
    YOLO26_640 = ESP32P4_DET_YOLO26_640,
    YOLO26_512 = ESP32P4_DET_YOLO26_512,
  };

  static const int kMaxResults = 32;

  ESP32P4_ObjectDetect() = default;
  ~ESP32P4_ObjectDetect() { end(); }

  bool begin(Model model = COCO_YOLO11N);
  void end();
  bool ready() const { return _impl != nullptr || _yolo26 != nullptr; }

  bool setModel(Model m);
  Model model() const { return _model; }

  void setScoreThr(float thr);
  float scoreThr() const { return _score_thr; }

  /** RGB565 packed pixels → boxes in the same pixel space. Fills out[i].label. */
  int detect(const uint16_t *rgb565, int w, int h, esp32p4_det_t *out, int max_out);
  int detect(const camera_fb_t *fb, esp32p4_det_t *out, int max_out);
  /** Packed RGB888 (R,G,B per pixel). */
  int detectRgb888(const uint8_t *rgb888, int w, int h, esp32p4_det_t *out, int max_out);
  /** JPEG → RGB565 decode via an already-begun ESP32P4_Jpeg, then detect. */
  int detectJpeg(ESP32P4_Jpeg &jpeg, const uint8_t *jpg, size_t jpg_len, esp32p4_det_t *out,
                 int max_out);

  /** Same as detect(), but stores into results() (up to kMaxResults). */
  int infer(const uint16_t *rgb565, int w, int h);
  int infer(const camera_fb_t *fb);
  int inferRgb888(const uint8_t *rgb888, int w, int h);
  int inferJpeg(ESP32P4_Jpeg &jpeg, const uint8_t *jpg, size_t jpg_len);

  const esp32p4_det_t *results() const { return _last; }
  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }

  /** JSON of last infer/detect into buf. Returns bytes written. */
  size_t resultsJson(char *buf, size_t cap) const;
  static size_t toJson(const esp32p4_det_t *dets, int n, char *buf, size_t cap, int ms = -1);

  static const char *label(Model model, int category);
  const char *label(int category) const { return label(_model, category); }
  static const char *modelName(Model model);
  /** Filename under /models/p4/ (e.g. espdet_pico_224_224_dog.espdl). */
  static const char *modelFile(Model model);

  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_det_t *dets, int n, Model model,
                   uint16_t color = 0x07E0);

 private:
  enum Kind : int { KIND_NONE = 0, KIND_WRAPPER, KIND_YOLO26 };

  bool ensureRgb(size_t pixels);
  bool beginYolo26(Model model);
  int runRgb888(const uint8_t *rgb888, int dw, int dh, int src_w, int src_h, esp32p4_det_t *out,
                int max_out);
  void finishDets(esp32p4_det_t *out, int n);

  void *_impl = nullptr;     // DetectWrapper* (COCO / Ped / Cat / Dog / Hand)
  void *_dl_model = nullptr; // dl::Model* for YOLO26
  void *_yolo26 = nullptr;   // YOLO26*
  Kind _kind = KIND_NONE;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  Model _model = COCO_YOLO11N;
  float _score_thr = 0.25f;
  int _last_ms = 0;
  int _last_n = 0;
  esp32p4_det_t _last[kMaxResults] = {};
};
