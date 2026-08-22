#include "detect/ESP32P4_Ocr.h"

#include <new>
#include <stdio.h>
#include <string.h>

#include "cv/ESP32P4_Cv.h"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "pp_ocr_v6.hpp"
#include "storage/esp32p4_model_mount.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_Ocr";

bool ESP32P4_Ocr::begin(RecModel rec, RecMode mode) {
  end();
  auto r = (rec == REC_S8) ? pp_ocr_v6::RecModel::PP_OCR_V6_REC_S8 : pp_ocr_v6::RecModel::PP_OCR_V6_REC_S16;
  auto m = (mode == DUAL) ? pp_ocr_v6::RecMode::Dual : pp_ocr_v6::RecMode::Short;
  const char *rec_file = (rec == REC_S8) ? "pp_ocr_v6_rec_s8.espdl" : "pp_ocr_v6_rec_s16.espdl";
  const char *pair[2] = {"pp_ocr_v6_det_s8.espdl", rec_file};
  if (!esp32p4_locate_models_p4_n(pair, 2)) {
    ESP_LOGW(TAG, "OCR .espdl not found");
    return false;
  }
  ESP_LOGI(TAG, "load rec=%d  SPIRAM free=%u largest=%u", (int)rec,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  auto *ocr = new (std::nothrow) pp_ocr_v6::PPOCRV6(m, r);
  if (!ocr) {
    ESP_LOGE(TAG, "PPOCRV6 alloc failed");
    return false;
  }
  if (!ocr->ok()) {
    ESP_LOGE(TAG, "PPOCRV6 model load failed rec=%d", (int)rec);
    delete ocr;
    return false;
  }
  _impl = ocr;
  _score_thr = pp_ocr_v6::PPOCRV6::default_rec_score_threshold;
  ocr->set_rec_score_threshold(_score_thr);
  ESP_LOGI(TAG, "ready rec=%d mode=%d  SPIRAM free=%u largest=%u", (int)rec, (int)mode,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  return true;
}

void ESP32P4_Ocr::end() {
  if (_impl) {
    delete static_cast<pp_ocr_v6::PPOCRV6 *>(_impl);
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

void ESP32P4_Ocr::setScoreThr(float thr) {
  if (thr < 0.01f) thr = 0.01f;
  if (thr > 0.99f) thr = 0.99f;
  _score_thr = thr;
  if (_impl) static_cast<pp_ocr_v6::PPOCRV6 *>(_impl)->set_rec_score_threshold(thr);
}

bool ESP32P4_Ocr::ensureRgb(size_t pixels) {
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

int ESP32P4_Ocr::run(const camera_fb_t *fb, esp32p4_ocr_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return run((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

int ESP32P4_Ocr::run(const uint16_t *rgb565, int w, int h, esp32p4_ocr_t *out, int max_out) {
  if (!ready() || !rgb565 || !out || max_out <= 0 || w <= 0 || h <= 0) return 0;
  if (!ensureRgb((size_t)w * (size_t)h)) return 0;

  ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, (size_t)w * (size_t)h);
  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  const int64_t t0 = esp_timer_get_time();
  auto results = static_cast<pp_ocr_v6::PPOCRV6 *>(_impl)->run(img);
  int n = 0;
  for (const auto &res : results) {
    if (n >= max_out) break;
    memset(&out[n], 0, sizeof(out[n]));
    for (int i = 0; i < 8; i++) out[n].points[i] = res.box.points[i];
    out[n].score = res.score;
    strncpy(out[n].text, res.text.c_str(), sizeof(out[n].text) - 1);
    n++;
  }
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  _last_n = n;
  return n;
}

void ESP32P4_Ocr::draw(uint16_t *rgb565, int w, int h, const esp32p4_ocr_t *hits, int n, uint16_t color) {
  if (!rgb565 || !hits || n <= 0) return;
  const uint16_t col_text = 0xEF5D;
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < 4; k++) {
      int x0 = hits[i].points[k * 2];
      int y0 = hits[i].points[k * 2 + 1];
      int x1 = hits[i].points[((k + 1) % 4) * 2];
      int y1 = hits[i].points[((k + 1) % 4) * 2 + 1];
      ESP32P4_Cv::line(rgb565, w, h, x0, y0, x1, y1, color, 1);
    }
    if (hits[i].text[0]) {
      ESP32P4_Cv::putText(rgb565, w, h, hits[i].points[0], hits[i].points[1] - 10, hits[i].text, col_text, 1);
    }
  }
}
