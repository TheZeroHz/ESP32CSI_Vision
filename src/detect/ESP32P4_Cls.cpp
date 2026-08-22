#include "detect/ESP32P4_Cls.h"

#include <new>

#include "imagenet_cls.hpp"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "storage/esp32p4_model_mount.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_Cls";

bool ESP32P4_Cls::begin(int topk) {
  end();
  if (topk < 1) topk = 1;
  if (topk > 10) topk = 10;
  _topk = topk;
  if (!esp32p4_locate_models_p4("imagenet_cls_mobilenetv2_s8_v1.espdl")) {
    ESP_LOGW(TAG, "imagenet_cls_mobilenetv2_s8_v1.espdl not found");
    return false;
  }
  auto *cls = new (std::nothrow) ImageNetCls(ImageNetCls::MOBILENETV2_S8_V1, true);
  if (!cls) {
    ESP_LOGE(TAG, "ImageNetCls alloc failed");
    return false;
  }
  cls->set_topk(_topk);
  _impl = cls;
  ESP_LOGI(TAG, "ready ImageNet MobileNetV2 topk=%d", _topk);
  return true;
}

void ESP32P4_Cls::end() {
  if (_impl) {
    delete static_cast<ImageNetCls *>(_impl);
    _impl = nullptr;
  }
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  _last_ms = 0;
}

void ESP32P4_Cls::setTopk(int k) {
  if (k < 1) k = 1;
  if (k > 10) k = 10;
  if (k == _topk) return;
  _topk = k;
  if (_impl) begin(_topk);
}

bool ESP32P4_Cls::ensureRgb(size_t pixels) {
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

int ESP32P4_Cls::classify(const camera_fb_t *fb, esp32p4_cls_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return classify((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

int ESP32P4_Cls::classify(const uint16_t *rgb565, int w, int h, esp32p4_cls_t *out, int max_out) {
  if (!ready() || !rgb565 || !out || max_out <= 0 || w <= 0 || h <= 0) return 0;
  if (!ensureRgb((size_t)w * (size_t)h)) return 0;

  ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, (size_t)w * (size_t)h);
  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  const int64_t t0 = esp_timer_get_time();
  auto &results = static_cast<ImageNetCls *>(_impl)->run(img);
  int n = 0;
  for (const auto &res : results) {
    if (n >= max_out) break;
    out[n].label = res.cat_name ? res.cat_name : "?";
    out[n].score = res.score;
    n++;
  }
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  return n;
}
