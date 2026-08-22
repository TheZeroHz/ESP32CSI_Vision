#pragma once

/**
 * ESP-DL COCO-17 pose (YOLO11n-Pose). Weights: models/espdl/p4/coco_pose_*.espdl
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "vision/ESP32P4_VisionAi.h"

enum esp32p4_pose_model_t : int {
  ESP32P4_POSE_YOLO11N_V1 = 0,
  ESP32P4_POSE_YOLO11N_V2 = 1,
};

class ESP32P4_Pose {
 public:
  enum Model : int {
    YOLO11N_POSE_V1 = ESP32P4_POSE_YOLO11N_V1,
    YOLO11N_POSE_V2 = ESP32P4_POSE_YOLO11N_V2,
  };

  ESP32P4_Pose() = default;
  ~ESP32P4_Pose() { end(); }

  bool begin(Model model = YOLO11N_POSE_V2);
  void end();
  bool ready() const { return _impl != nullptr; }

  bool setModel(Model m);
  Model model() const { return _model; }

  void setScoreThr(float thr);
  float scoreThr() const { return _score_thr; }

  int detect(const uint16_t *rgb565, int w, int h, esp32p4_pose_t *out, int max_out);
  int detect(const camera_fb_t *fb, esp32p4_pose_t *out, int max_out);

  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }

  static const char *modelName(Model model);
  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_pose_t *poses, int n,
                   uint16_t color = 0x07E0);

 private:
  bool ensureRgb(size_t pixels);

  void *_impl = nullptr;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  Model _model = YOLO11N_POSE_V2;
  float _score_thr = 0.25f;
  int _last_ms = 0;
  int _last_n = 0;
};
