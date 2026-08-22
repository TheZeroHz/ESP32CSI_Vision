#include "detect/ESP32P4_Pose.h"

#include <new>
#include <stdio.h>
#include <string.h>

#include "coco_pose.hpp"
#include "cv/ESP32P4_Cv.h"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "storage/esp32p4_model_mount.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_Pose";

static const int kSkeleton[][2] = {
    {0, 1},  {0, 2},  {1, 3},  {2, 4},  {5, 6},  {5, 7},  {7, 9},  {6, 8},
    {8, 10}, {5, 11}, {6, 12}, {11, 12}, {11, 13}, {13, 15}, {12, 14}, {14, 16},
};

const char *ESP32P4_Pose::modelName(Model model) {
  return (model == YOLO11N_POSE_V1) ? "YOLO11n-Pose v1" : "YOLO11n-Pose v2";
}

bool ESP32P4_Pose::begin(Model model) {
  end();
  _model = model;
  auto t = (model == YOLO11N_POSE_V1) ? COCOPose::YOLO11N_POSE_S8_V1 : COCOPose::YOLO11N_POSE_S8_V2;
  const char *fname = (model == YOLO11N_POSE_V1) ? "coco_pose_yolo11n_pose_s8_v1.espdl"
                                                : "coco_pose_yolo11n_pose_s8_v2.espdl";
  if (!esp32p4_locate_models_p4(fname)) {
    ESP_LOGW(TAG, "%s not found", fname);
    return false;
  }
  auto *det = new (std::nothrow) COCOPose(t, false);
  if (!det) {
    ESP_LOGE(TAG, "COCOPose alloc failed");
    return false;
  }
  _impl = det;
  _score_thr = coco_pose::Yolo11nPose::default_score_thr;
  ESP_LOGI(TAG, "ready model=%s", modelName(model));
  return true;
}

void ESP32P4_Pose::end() {
  if (_impl) {
    delete static_cast<COCOPose *>(_impl);
    _impl = nullptr;
  }
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  _last_ms = 0;
  _last_n = 0;
}

bool ESP32P4_Pose::setModel(Model m) {
  if (ready() && m == _model) return true;
  return begin(m);
}

void ESP32P4_Pose::setScoreThr(float thr) {
  if (thr < 0.01f) thr = 0.01f;
  if (thr > 0.99f) thr = 0.99f;
  _score_thr = thr;
  if (_impl) static_cast<COCOPose *>(_impl)->set_score_thr(thr, 0);
}

bool ESP32P4_Pose::ensureRgb(size_t pixels) {
  const size_t need = pixels * 3;
  if (need <= _rgb888_cap) return true;
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  _rgb888 = (uint8_t *)esp32p4_psram_alloc(need);
  if (!_rgb888) return false;
  _rgb888_cap = need;
  return true;
}

int ESP32P4_Pose::detect(const camera_fb_t *fb, esp32p4_pose_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return detect((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

int ESP32P4_Pose::detect(const uint16_t *rgb565, int w, int h, esp32p4_pose_t *out, int max_out) {
  if (!ready() || !rgb565 || !out || max_out <= 0 || w <= 0 || h <= 0) return 0;
  if (!ensureRgb((size_t)w * (size_t)h)) return 0;

  ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, (size_t)w * (size_t)h);
  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  const int64_t t0 = esp_timer_get_time();
  auto &results = static_cast<COCOPose *>(_impl)->run(img);
  int n = 0;
  for (const auto &res : results) {
    if (n >= max_out) break;
    if (res.box.size() < 4) continue;
    memset(&out[n], 0, sizeof(out[n]));
    out[n].box.score = res.score;
    out[n].box.category = res.category;
    out[n].box.x = (int)res.box[0];
    out[n].box.y = (int)res.box[1];
    out[n].box.w = (int)(res.box[2] - res.box[0]);
    out[n].box.h = (int)(res.box[3] - res.box[1]);
    out[n].box.label = "person";
    const int nk = (int)res.keypoint.size() / 2;
    for (int k = 0; k < 17 && k < nk; k++) {
      out[n].kp[k].x = (float)res.keypoint[2 * k];
      out[n].kp[k].y = (float)res.keypoint[2 * k + 1];
      out[n].kp[k].score = (out[n].kp[k].x > 0.f || out[n].kp[k].y > 0.f) ? 1.f : 0.f;
    }
    n++;
  }
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  _last_n = n;
  return n;
}

void ESP32P4_Pose::draw(uint16_t *rgb565, int w, int h, const esp32p4_pose_t *poses, int n, uint16_t color) {
  if (!rgb565 || !poses || n <= 0) return;
  const uint16_t bone = 0x07FF;
  for (int i = 0; i < n; i++) {
    esp32p4_rect_t r{poses[i].box.x, poses[i].box.y, poses[i].box.w, poses[i].box.h};
    ESP32P4_Img::fillRect565(rgb565, w, h, r, color, 2);
    for (size_t e = 0; e < sizeof(kSkeleton) / sizeof(kSkeleton[0]); e++) {
      const auto &a = poses[i].kp[kSkeleton[e][0]];
      const auto &b = poses[i].kp[kSkeleton[e][1]];
      if (a.score <= 0.f || b.score <= 0.f) continue;
      ESP32P4_Cv::line(rgb565, w, h, (int)a.x, (int)a.y, (int)b.x, (int)b.y, bone, 1);
    }
    for (int k = 0; k < 17; k++) {
      if (poses[i].kp[k].score <= 0.f) continue;
      ESP32P4_Cv::circle(rgb565, w, h, (int)poses[i].kp[k].x, (int)poses[i].kp[k].y, 2, color, 1);
    }
  }
}
