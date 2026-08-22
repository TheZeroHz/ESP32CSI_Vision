#include "detect/ESP32P4_Gesture.h"

#include <list>
#include <new>
#include <stdio.h>
#include <string.h>

#include "cv/ESP32P4_Cv.h"
#include "hand_detect.hpp"
#include "hand_gesture_recognition.hpp"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "storage/esp32p4_model_mount.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_Gesture";

bool ESP32P4_Gesture::begin() {
  end();
  const char *pair[2] = {"espdet_pico_224_224_hand.espdl", "mobilenetv2_0_5_128_128_gesture.espdl"};
  if (!esp32p4_locate_models_p4_n(pair, 2)) {
    ESP_LOGW(TAG, "hand/gesture .espdl not found");
    return false;
  }
  auto *hand = new (std::nothrow) HandDetect(HandDetect::ESPDET_PICO_224_224_HAND, false);
  if (!hand) {
    ESP_LOGE(TAG, "HandDetect alloc failed");
    return false;
  }
  auto *cls = new (std::nothrow) HandGestureRecognizer();
  if (!cls) {
    delete hand;
    ESP_LOGE(TAG, "HandGestureRecognizer alloc failed");
    return false;
  }
  _hand = hand;
  _cls = cls;
  _score_thr = hand_detect::ESPDet::default_score_thr;
  ESP_LOGI(TAG, "ready hand+gesture");
  return true;
}

void ESP32P4_Gesture::end() {
  if (_cls) {
    delete static_cast<HandGestureRecognizer *>(_cls);
    _cls = nullptr;
  }
  if (_hand) {
    delete static_cast<HandDetect *>(_hand);
    _hand = nullptr;
  }
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  _last_ms = 0;
  _last_n = 0;
}

void ESP32P4_Gesture::setScoreThr(float thr) {
  if (thr < 0.01f) thr = 0.01f;
  if (thr > 0.99f) thr = 0.99f;
  _score_thr = thr;
  if (_hand) static_cast<HandDetect *>(_hand)->set_score_thr(thr, 0);
}

bool ESP32P4_Gesture::ensureRgb(size_t pixels) {
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

int ESP32P4_Gesture::detect(const camera_fb_t *fb, esp32p4_gesture_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return detect((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

int ESP32P4_Gesture::detect(const uint16_t *rgb565, int w, int h, esp32p4_gesture_t *out, int max_out) {
  if (!ready() || !rgb565 || !out || max_out <= 0 || w <= 0 || h <= 0) return 0;
  if (!ensureRgb((size_t)w * (size_t)h)) return 0;

  ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, (size_t)w * (size_t)h);
  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  const int64_t t0 = esp_timer_get_time();
  auto &hands = static_cast<HandDetect *>(_hand)->run(img);
  auto gestures = static_cast<HandGestureRecognizer *>(_cls)->recognize(img, hands);

  int n = 0;
  auto git = gestures.begin();
  for (const auto &hand : hands) {
    if (n >= max_out) break;
    if (hand.box.size() < 4) continue;
    memset(&out[n], 0, sizeof(out[n]));
    out[n].hand.score = hand.score;
    out[n].hand.category = hand.category;
    out[n].hand.x = (int)hand.box[0];
    out[n].hand.y = (int)hand.box[1];
    out[n].hand.w = (int)(hand.box[2] - hand.box[0]);
    out[n].hand.h = (int)(hand.box[3] - hand.box[1]);
    out[n].hand.label = "hand";
    if (git != gestures.end()) {
      out[n].label = git->cat_name ? git->cat_name : "?";
      out[n].score = git->score;
      ++git;
    } else {
      out[n].label = "no_gesture";
      out[n].score = 0.f;
    }
    n++;
  }
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  _last_n = n;
  return n;
}

void ESP32P4_Gesture::draw(uint16_t *rgb565, int w, int h, const esp32p4_gesture_t *g, int n, uint16_t color) {
  if (!rgb565 || !g || n <= 0) return;
  const uint16_t col_plate = 0x0841;
  const uint16_t col_text = 0xEF5D;
  for (int i = 0; i < n; i++) {
    esp32p4_rect_t r{g[i].hand.x, g[i].hand.y, g[i].hand.w, g[i].hand.h};
    ESP32P4_Img::fillRect565(rgb565, w, h, r, color, 2);
    char buf[40];
    int pct = (int)(g[i].score * 100.f + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 99) pct = 99;
    snprintf(buf, sizeof(buf), "%s %d%%", g[i].label ? g[i].label : "?", pct);
    const int tw = (int)strlen(buf) * 6;
    int lx = g[i].hand.x;
    int ly = g[i].hand.y - 12;
    if (ly < 0) ly = g[i].hand.y + 2;
    if (lx + tw + 4 > w) lx = w - tw - 4;
    if (lx < 0) lx = 0;
    for (int yy = ly; yy < ly + 11 && yy < h; yy++) {
      if (yy < 0) continue;
      for (int xx = lx; xx < lx + tw + 4 && xx < w; xx++) {
        if (xx < 0) continue;
        rgb565[yy * w + xx] = col_plate;
      }
    }
    ESP32P4_Cv::putText(rgb565, w, h, lx + 2, ly + 2, buf, col_text, 1);
  }
}
