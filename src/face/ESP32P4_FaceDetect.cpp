#include "face/ESP32P4_FaceDetect.h"

#include <new>

#include "human_face_detect.hpp"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "storage/esp32p4_model_mount.h"

#include "esp_log.h"

static const char *TAG = "ESP32P4_FaceDetect";

bool ESP32P4_FaceDetect::begin(Model model) {
  end();
  _model = model;

  if (model == ESPDET_PICO_224) {
    if (!esp32p4_locate_models_p4("espdet_pico_224_224_face.espdl")) {
      ESP_LOGW(TAG, "espdet_pico_224_224_face.espdl not found");
      return false;
    }
  } else if (model == ESPDET_PICO_416) {
    if (!esp32p4_locate_models_p4("espdet_pico_416_416_face.espdl")) {
      ESP_LOGW(TAG, "espdet_pico_416_416_face.espdl not found");
      return false;
    }
  } else {
    const char *pair[2] = {"human_face_detect_msr_s8_v1.espdl", "human_face_detect_mnp_s8_v1.espdl"};
    if (!esp32p4_locate_models_p4_n(pair, 2)) {
      ESP_LOGW(TAG, "MSR/MNP face .espdl not found");
      return false;
    }
  }

  HumanFaceDetect::model_type_t t = HumanFaceDetect::MSRMNP_S8_V1;
  switch (model) {
    case ESPDET_PICO_224:
      t = HumanFaceDetect::ESPDET_PICO_224_224_FACE;
      break;
    case ESPDET_PICO_416:
      t = HumanFaceDetect::ESPDET_PICO_416_416_FACE;
      break;
    case MSRMNP_S8_V1:
    default:
      t = HumanFaceDetect::MSRMNP_S8_V1;
      break;
  }

  auto *det = new (std::nothrow) HumanFaceDetect(t, /*lazy_load=*/false);
  if (!det) {
    ESP_LOGE(TAG, "HumanFaceDetect alloc failed");
    return false;
  }
  _impl = det;
  ESP_LOGI(TAG, "HumanFaceDetect ready (model=%d)", (int)model);
  return true;
}

void ESP32P4_FaceDetect::end() {
  if (_impl) {
    delete static_cast<HumanFaceDetect *>(_impl);
    _impl = nullptr;
  }
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
}

int ESP32P4_FaceDetect::detect(const camera_fb_t *fb, esp32p4_face_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return detect((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

int ESP32P4_FaceDetect::detect(const uint16_t *rgb565, int w, int h, esp32p4_face_t *out, int max_out) {
  if (!_impl || !rgb565 || !out || max_out <= 0 || w <= 0 || h <= 0) {
    return 0;
  }

  const size_t pixels = (size_t)w * (size_t)h;
  const size_t need = pixels * 3;
  if (need > _rgb888_cap) {
    if (_rgb888) {
      esp32p4_psram_free(_rgb888);
      _rgb888 = nullptr;
      _rgb888_cap = 0;
    }
    _rgb888 = (uint8_t *)esp32p4_psram_alloc(need);
    if (!_rgb888) {
      ESP_LOGE(TAG, "RGB888 buffer alloc failed (%u)", (unsigned)need);
      return 0;
    }
    _rgb888_cap = need;
  }

  ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, pixels);

  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  auto *det = static_cast<HumanFaceDetect *>(_impl);
  auto &results = det->run(img);

  int n = 0;
  for (const auto &res : results) {
    if (n >= max_out) {
      break;
    }
    if (res.box.size() < 4) {
      continue;
    }
    esp32p4_face_t &f = out[n];
    f.score = res.score;
    f.x = (int)res.box[0];
    f.y = (int)res.box[1];
    f.w = (int)(res.box[2] - res.box[0]);
    f.h = (int)(res.box[3] - res.box[1]);
    f.has_landmarks = false;
    if (res.keypoint.size() >= 10) {
      f.has_landmarks = true;
      for (int i = 0; i < 5; i++) {
        f.landmarks[i][0] = (int)res.keypoint[i * 2];
        f.landmarks[i][1] = (int)res.keypoint[i * 2 + 1];
      }
    }
    n++;
  }
  return n;
}

void ESP32P4_FaceDetect::draw(uint16_t *rgb565, int w, int h, const esp32p4_face_t *faces, int n,
                              uint16_t color) {
  if (!rgb565 || !faces || n <= 0) return;
  for (int i = 0; i < n; i++) {
    esp32p4_rect_t box{faces[i].x, faces[i].y, faces[i].w, faces[i].h};
    ESP32P4_Img::fillRect565(rgb565, w, h, box, color, 2);
  }
}

