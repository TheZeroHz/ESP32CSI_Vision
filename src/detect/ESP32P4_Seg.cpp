#include "detect/ESP32P4_Seg.h"

#include <new>
#include <string.h>

#include "coco_seg.hpp"
#include "detect/ESP32P4_ObjectDetect.h"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "storage/esp32p4_model_mount.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_Seg";

static uint16_t tint565(int category) {
  static const uint16_t k[8] = {0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F, 0x07FF, 0xFD20, 0x8410};
  return k[(category < 0 ? 0 : category) & 7];
}

const char *ESP32P4_Seg::label(int category) {
  return ESP32P4_ObjectDetect::label(ESP32P4_ObjectDetect::COCO_YOLO11N, category);
}

bool ESP32P4_Seg::begin(Model model) {
  (void)model;
  end();
  if (!esp32p4_locate_models_p4("coco_seg_yolo11n_seg_s8_v1.espdl")) {
    ESP_LOGW(TAG, "coco_seg_yolo11n_seg_s8_v1.espdl not found");
    return false;
  }
  const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  if (largest < 6 * 1024 * 1024) {
    ESP_LOGW(TAG, "need ~6MB contiguous PSRAM for YOLO11n-Seg, largest=%u KB",
             (unsigned)(largest / 1024));
    return false;
  }
  auto *det = new (std::nothrow) COCOSeg(COCOSeg::YOLO11N_SEG_S8_V1, false);
  if (!det) {
    ESP_LOGE(TAG, "COCOSeg alloc failed");
    return false;
  }
  dl::Model *raw = det->get_raw_model();
  if (!raw || !raw->get_input()) {
    ESP_LOGE(TAG, "COCOSeg model load failed");
    delete det;
    return false;
  }
  _impl = det;
  _score_thr = coco_seg::Yolo11nSeg::default_score_thr;
  ESP_LOGI(TAG, "ready YOLO11n-Seg");
  return true;
}

void ESP32P4_Seg::end() {
  if (_impl) {
    delete static_cast<COCOSeg *>(_impl);
    _impl = nullptr;
  }
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  if (_mask_arena) {
    esp32p4_psram_free(_mask_arena);
    _mask_arena = nullptr;
    _mask_arena_cap = 0;
  }
  _last_ms = 0;
  _last_n = 0;
}

void ESP32P4_Seg::setScoreThr(float thr) {
  if (thr < 0.01f) thr = 0.01f;
  if (thr > 0.99f) thr = 0.99f;
  _score_thr = thr;
  if (_impl) static_cast<COCOSeg *>(_impl)->set_score_thr(thr, 0);
}

bool ESP32P4_Seg::ensureRgb(size_t pixels) {
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

int ESP32P4_Seg::detect(const camera_fb_t *fb, esp32p4_seg_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return detect((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

int ESP32P4_Seg::detect(const uint16_t *rgb565, int w, int h, esp32p4_seg_t *out, int max_out) {
  if (!ready() || !rgb565 || !out || max_out <= 0 || w <= 0 || h <= 0) return 0;
  if (!ensureRgb((size_t)w * (size_t)h)) return 0;

  ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, (size_t)w * (size_t)h);
  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = w;
  img.height = h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  const int64_t t0 = esp_timer_get_time();
  auto &results = static_cast<COCOSeg *>(_impl)->run(img);

  size_t mask_bytes = 0;
  for (const auto &res : results) mask_bytes += res.mask.size();
  if (mask_bytes > _mask_arena_cap) {
    if (_mask_arena) esp32p4_psram_free(_mask_arena);
    _mask_arena = (uint8_t *)esp32p4_psram_alloc(mask_bytes ? mask_bytes : 1);
    _mask_arena_cap = _mask_arena ? mask_bytes : 0;
  }

  int n = 0;
  size_t off = 0;
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
    out[n].box.label = ESP32P4_ObjectDetect::label(ESP32P4_ObjectDetect::COCO_YOLO11N, res.category);
    if (!res.mask.empty() && _mask_arena && off + res.mask.size() <= _mask_arena_cap) {
      memcpy(_mask_arena + off, res.mask.data(), res.mask.size());
      out[n].mask = _mask_arena + off;
      if (out[n].box.w > 0 && res.mask.size() == (size_t)out[n].box.w * (size_t)out[n].box.h) {
        out[n].mask_w = out[n].box.w;
        out[n].mask_h = out[n].box.h;
      } else {
        out[n].mask_w = (int)res.mask.size();
        out[n].mask_h = 1;
      }
      off += res.mask.size();
    }
    n++;
  }
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  _last_n = n;
  return n;
}

void ESP32P4_Seg::draw(uint16_t *rgb565, int w, int h, const esp32p4_seg_t *dets, int n) {
  if (!rgb565 || !dets || n <= 0) return;
  for (int i = 0; i < n; i++) {
    const uint16_t col = tint565(dets[i].box.category);
    if (dets[i].mask && dets[i].mask_w > 0 && dets[i].mask_h > 0) {
      for (int yy = 0; yy < dets[i].mask_h; yy++) {
        int iy = dets[i].box.y + yy;
        if (iy < 0 || iy >= h) continue;
        for (int xx = 0; xx < dets[i].mask_w; xx++) {
          if (!dets[i].mask[yy * dets[i].mask_w + xx]) continue;
          int ix = dets[i].box.x + xx;
          if (ix < 0 || ix >= w) continue;
          uint16_t p = rgb565[iy * w + ix];
          rgb565[iy * w + ix] = (uint16_t)(((p & 0xF7DE) >> 1) + ((col & 0xF7DE) >> 1));
        }
      }
    }
    esp32p4_rect_t r{dets[i].box.x, dets[i].box.y, dets[i].box.w, dets[i].box.h};
    ESP32P4_Img::fillRect565(rgb565, w, h, r, col, 2);
  }
}
