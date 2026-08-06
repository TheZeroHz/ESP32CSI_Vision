#include "ESP32P4_FaceDetect.h"

#include <new>

#include "human_face_detect.hpp"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"

#include "esp_log.h"

static const char *TAG = "ESP32P4_FaceDetect";

bool ESP32P4_FaceDetect::begin(Model model) {
  end();
  _model = model;

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
  if (!_impl || !fb || !fb->buf || !out || max_out <= 0) {
    return 0;
  }

  const size_t pixels = (size_t)fb->width * (size_t)fb->height;
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

  ESP32P4_Img::rgb565ToRgb888((const uint16_t *)fb->buf, _rgb888, pixels);

  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = fb->width;
  img.height = fb->height;
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
    f.x = res.box[0];
    f.y = res.box[1];
    f.w = res.box[2] - res.box[0];
    f.h = res.box[3] - res.box[1];
    f.has_landmarks = false;
    if (res.keypoint.size() >= 10) {
      f.has_landmarks = true;
      for (int i = 0; i < 5; i++) {
        f.landmarks[i][0] = res.keypoint[i * 2];
        f.landmarks[i][1] = res.keypoint[i * 2 + 1];
      }
    }
    n++;
  }
  return n;
}
