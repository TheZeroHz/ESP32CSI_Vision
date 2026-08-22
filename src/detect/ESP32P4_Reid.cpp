#include "detect/ESP32P4_Reid.h"

#include <list>
#include <new>
#include <stdio.h>
#include <string.h>
#include <string>

#include "cv/ESP32P4_Cv.h"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "pedestrian_detect.hpp"
#include "person_reid.hpp"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_Reid";

bool ESP32P4_Reid::begin(const char *db_path) {
  end();
  if (!db_path || !db_path[0]) {
    ESP_LOGE(TAG, "db_path required");
    return false;
  }
  auto *det = new (std::nothrow) PedestrianDetect(PedestrianDetect::PICO_S8_V1, false);
  if (!det) return false;
  auto *match = new (std::nothrow) PersonReidMatcher(std::string(db_path), PersonReidFeat::OSN_S8_V1, false);
  if (!match) {
    delete det;
    return false;
  }
  match->set_thresh(_thr);
  _det = det;
  _match = match;
  ESP_LOGI(TAG, "ready db=%s feats=%d", db_path, match->get_num_feats());
  return true;
}

void ESP32P4_Reid::end() {
  if (_match) {
    delete static_cast<PersonReidMatcher *>(_match);
    _match = nullptr;
  }
  if (_det) {
    delete static_cast<PedestrianDetect *>(_det);
    _det = nullptr;
  }
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  _last_ms = 0;
  _last_n = 0;
}

void ESP32P4_Reid::setThresh(float thr) {
  if (thr < 0.01f) thr = 0.01f;
  if (thr > 0.99f) thr = 0.99f;
  _thr = thr;
  if (_match) static_cast<PersonReidMatcher *>(_match)->set_thresh(thr);
}

int ESP32P4_Reid::featCount() const {
  if (!_match) return 0;
  return static_cast<PersonReidMatcher *>(_match)->get_num_feats();
}

bool ESP32P4_Reid::clearDb() {
  if (!_match) return false;
  return static_cast<PersonReidMatcher *>(_match)->clear_all_feats() == ESP_OK;
}

bool ESP32P4_Reid::deleteId(uint16_t id) {
  if (!_match) return false;
  return static_cast<PersonReidMatcher *>(_match)->delete_feat(id) == ESP_OK;
}

bool ESP32P4_Reid::ensureRgb(size_t pixels) {
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

bool ESP32P4_Reid::toImg(const uint16_t *rgb565, int w, int h) {
  if (!rgb565 || w <= 0 || h <= 0) return false;
  if (!ensureRgb((size_t)w * (size_t)h)) return false;
  ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, (size_t)w * (size_t)h);
  _img_w = w;
  _img_h = h;
  return true;
}

int ESP32P4_Reid::run(const camera_fb_t *fb, esp32p4_reid_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return run((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

int ESP32P4_Reid::run(const uint16_t *rgb565, int w, int h, esp32p4_reid_t *out, int max_out) {
  if (!ready() || !out || max_out <= 0) return 0;
  if (!toImg(rgb565, w, h)) return 0;

  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  const int64_t t0 = esp_timer_get_time();
  auto &dets = static_cast<PedestrianDetect *>(_det)->run(img);
  auto *match = static_cast<PersonReidMatcher *>(_match);

  int n = 0;
  for (const auto &d : dets) {
    if (n >= max_out) break;
    if (d.box.size() < 4) continue;
    memset(&out[n], 0, sizeof(out[n]));
    out[n].box.score = d.score;
    out[n].box.category = d.category;
    out[n].box.x = (int)d.box[0];
    out[n].box.y = (int)d.box[1];
    out[n].box.w = (int)(d.box[2] - d.box[0]);
    out[n].box.h = (int)(d.box[3] - d.box[1]);
    out[n].id = -1;
    out[n].similarity = 0.f;
    std::list<dl::detect::result_t> one = {d};
    auto hits = match->recognize(img, one);
    if (!hits.empty()) {
      out[n].id = (int)hits[0].id;
      out[n].similarity = hits[0].similarity;
    }
    n++;
  }
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  _last_n = n;
  return n;
}

int ESP32P4_Reid::enroll(const uint16_t *rgb565, int w, int h) {
  if (!ready()) return -1;
  if (!toImg(rgb565, w, h)) return -1;
  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
  auto &dets = static_cast<PedestrianDetect *>(_det)->run(img);
  auto *match = static_cast<PersonReidMatcher *>(_match);
  if (match->enroll(img, dets) != ESP_OK) return -1;
  return (int)match->get_last_feat_id();
}

void ESP32P4_Reid::draw(uint16_t *rgb565, int w, int h, const esp32p4_reid_t *dets, int n, uint16_t color) {
  if (!rgb565 || !dets || n <= 0) return;
  const uint16_t col_plate = 0x0841;
  const uint16_t col_text = 0xEF5D;
  for (int i = 0; i < n; i++) {
    esp32p4_rect_t r{dets[i].box.x, dets[i].box.y, dets[i].box.w, dets[i].box.h};
    ESP32P4_Img::fillRect565(rgb565, w, h, r, color, 2);
    char buf[32];
    if (dets[i].id >= 0) {
      snprintf(buf, sizeof(buf), "id %d %.2f", dets[i].id, (double)dets[i].similarity);
    } else {
      snprintf(buf, sizeof(buf), "unknown");
    }
    ESP32P4_Cv::putText(rgb565, w, h, dets[i].box.x + 2, dets[i].box.y + 2, buf, col_text, 1);
    (void)col_plate;
  }
}
