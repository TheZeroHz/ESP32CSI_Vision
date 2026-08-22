#include "detect/ESP32P4_ObjectDetect.h"

#include <filesystem>
#include <algorithm>
#include <new>
#include <stdio.h>
#include <string.h>

#include "cat_detect.hpp"
#include "coco_detect.hpp"
#include "cv/ESP32P4_Cv.h"
#include "dog_detect.hpp"
#include "hand_detect.hpp"
#include "img/ESP32P4_Img.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "mem/ESP32P4_Psram.h"
#include "pedestrian_detect.hpp"
#include "storage/esp32p4_model_mount.h"
#include "yolo26.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_ObjectDetect";

static const char *kCocoLabels[80] = {
    "person",         "bicycle",    "car",           "motorcycle",    "airplane",     "bus",
    "train",          "truck",      "boat",          "traffic light", "fire hydrant", "stop sign",
    "parking meter",  "bench",      "bird",          "cat",           "dog",          "horse",
    "sheep",          "cow",        "elephant",      "bear",          "zebra",        "giraffe",
    "backpack",       "umbrella",   "handbag",       "tie",           "suitcase",     "frisbee",
    "skis",           "snowboard",  "sports ball",   "kite",          "baseball bat", "baseball glove",
    "skateboard",     "surfboard",  "tennis racket", "bottle",        "wine glass",   "cup",
    "fork",           "knife",      "spoon",         "bowl",          "banana",       "apple",
    "sandwich",       "orange",     "broccoli",      "carrot",        "hot dog",      "pizza",
    "donut",          "cake",       "chair",         "couch",         "potted plant", "bed",
    "dining table",   "toilet",     "tv",            "laptop",        "mouse",        "remote",
    "keyboard",       "cell phone", "microwave",     "oven",          "toaster",      "sink",
    "refrigerator",   "book",       "clock",         "vase",          "scissors",     "teddy bear",
    "hair drier",     "toothbrush",
};

const char *ESP32P4_ObjectDetect::modelName(Model model) {
  switch (model) {
    case COCO_YOLO11N:
      return "COCO YOLO11n 640";
    case COCO_YOLO11N_320:
      return "COCO YOLO11n 320";
    case PEDESTRIAN_PICO:
      return "Pedestrian Pico";
    case CAT_224:
      return "Cat 224";
    case CAT_416:
      return "Cat 416";
    case DOG_224:
      return "Dog 224";
    case DOG_416:
      return "Dog 416";
    case HAND_224:
      return "Hand 224";
    case YOLO26_640:
      return "YOLO26n 640";
    case YOLO26_512:
      return "YOLO26n 512";
    default:
      return "?";
  }
}

const char *ESP32P4_ObjectDetect::modelFile(Model model) {
  switch (model) {
    case COCO_YOLO11N:
      return "coco_detect_yolo11n_s8_v1.espdl";
    case COCO_YOLO11N_320:
      return "coco_detect_yolo11n_320_s8_v1.espdl";
    case PEDESTRIAN_PICO:
      return "pedestrian_detect_pico_s8_v1.espdl";
    case CAT_224:
      return "espdet_pico_224_224_cat.espdl";
    case CAT_416:
      return "espdet_pico_416_416_cat.espdl";
    case DOG_224:
      return "espdet_pico_224_224_dog.espdl";
    case DOG_416:
      return "espdet_pico_416_416_dog.espdl";
    case HAND_224:
      return "espdet_pico_224_224_hand.espdl";
    case YOLO26_640:
      return "yolo26n_640_s8_p4.espdl";
    case YOLO26_512:
      return "yolo26n_512_s8_p4.espdl";
    default:
      return "";
  }
}

const char *ESP32P4_ObjectDetect::label(Model model, int category) {
  switch (model) {
    case PEDESTRIAN_PICO:
      return "person";
    case CAT_224:
    case CAT_416:
      return "cat";
    case DOG_224:
    case DOG_416:
      return "dog";
    case HAND_224:
      return "hand";
    case COCO_YOLO11N:
    case COCO_YOLO11N_320:
    case YOLO26_640:
    case YOLO26_512:
    default:
      if (category < 0 || category >= 80) return "?";
      return kCocoLabels[category];
  }
}

bool ESP32P4_ObjectDetect::beginYolo26(Model model) {
  const char *fname =
      (model == YOLO26_512) ? "yolo26n_512_s8_p4.espdl" : "yolo26n_640_s8_p4.espdl";
  if (!esp32p4_locate_models_p4(fname)) {
    ESP_LOGW(TAG, "YOLO26 %s not found on any volume", fname);
  } else {
    ESP_LOGI(TAG, "YOLO26 %s on %s", fname, esp32p4_model_mount_point());
  }
  auto sd_path =
      std::filesystem::path(esp32p4_model_mount_point()) / "models/p4" / fname;
  ESP_LOGI(TAG, "YOLO26 load %s  PSRAM free=%u largest=%u", sd_path.c_str(),
           (unsigned)esp32p4_psram_free_size(),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  auto *mdl = new (std::nothrow) dl::Model(sd_path.c_str(), fbs::MODEL_LOCATION_IN_SDCARD);
  if (!mdl) {
    ESP_LOGE(TAG, "YOLO26 Model alloc failed");
    return false;
  }
  auto &ins = mdl->get_inputs();
  if (ins.empty() || !ins.begin()->second || !ins.begin()->second->data) {
    ESP_LOGE(TAG, "YOLO26 tensor alloc failed (need ~4MB contiguous PSRAM, largest=%u)",
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    delete mdl;
    return false;
  }
  mdl->minimize();
  auto *proc = new (std::nothrow) YOLO26(mdl, YOLO_TARGET_K, _score_thr, coco_classes);
  if (!proc) {
    delete mdl;
    ESP_LOGE(TAG, "YOLO26 processor alloc failed");
    return false;
  }
  _dl_model = mdl;
  _yolo26 = proc;
  _kind = KIND_YOLO26;
  return true;
}

bool ESP32P4_ObjectDetect::begin(Model model) {
  end();
  _model = model;

  const char *fname = nullptr;
  switch (model) {
    case YOLO26_640:
      fname = "yolo26n_640_s8_p4.espdl";
      break;
    case YOLO26_512:
      fname = "yolo26n_512_s8_p4.espdl";
      break;
    case COCO_YOLO11N:
      fname = "coco_detect_yolo11n_s8_v1.espdl";
      break;
    case COCO_YOLO11N_320:
      fname = "coco_detect_yolo11n_320_s8_v1.espdl";
      break;
    case PEDESTRIAN_PICO:
      fname = "pedestrian_detect_pico_s8_v1.espdl";
      break;
    case CAT_224:
      fname = "espdet_pico_224_224_cat.espdl";
      break;
    case CAT_416:
      fname = "espdet_pico_416_416_cat.espdl";
      break;
    case DOG_224:
      fname = "espdet_pico_224_224_dog.espdl";
      break;
    case DOG_416:
      fname = "espdet_pico_416_416_dog.espdl";
      break;
    case HAND_224:
      fname = "espdet_pico_224_224_hand.espdl";
      break;
    default:
      break;
  }
  if (fname) {
    if (esp32p4_locate_models_p4(fname)) {
      ESP_LOGI(TAG, "model %s on %s", fname, esp32p4_model_mount_point());
    } else {
      ESP_LOGW(TAG, "model %s not found on any volume", fname);
      return false;
    }
  }

  switch (model) {
    case YOLO26_640:
    case YOLO26_512:
      _score_thr = YOLO_CONF_THRESH;
      if (!beginYolo26(model)) return false;
      break;

    case COCO_YOLO11N:
    case COCO_YOLO11N_320: {
      _score_thr = coco_detect::Yolo11n::default_score_thr;
      auto t = (model == COCO_YOLO11N_320) ? COCODetect::YOLO11N_320_S8_V1 : COCODetect::YOLO11N_S8_V1;
      auto *det = new (std::nothrow) COCODetect(t, false);
      if (!det) return false;
      _impl = det;
      _kind = KIND_WRAPPER;
      break;
    }
    case PEDESTRIAN_PICO: {
      _score_thr = pedestrian_detect::Pico::default_score_thr;
      auto *det = new (std::nothrow) PedestrianDetect(PedestrianDetect::PICO_S8_V1, false);
      if (!det) return false;
      _impl = det;
      _kind = KIND_WRAPPER;
      break;
    }
    case CAT_224:
    case CAT_416: {
      _score_thr = cat_detect::ESPDet::default_score_thr;
      auto t = (model == CAT_416) ? CatDetect::ESPDET_PICO_416_416_CAT : CatDetect::ESPDET_PICO_224_224_CAT;
      auto *det = new (std::nothrow) CatDetect(t, false);
      if (!det) return false;
      _impl = det;
      _kind = KIND_WRAPPER;
      break;
    }
    case DOG_224:
    case DOG_416: {
      _score_thr = dog_detect::ESPDet::default_score_thr;
      auto t = (model == DOG_416) ? DogDetect::ESPDET_PICO_416_416_DOG : DogDetect::ESPDET_PICO_224_224_DOG;
      auto *det = new (std::nothrow) DogDetect(t, false);
      if (!det) return false;
      _impl = det;
      _kind = KIND_WRAPPER;
      break;
    }
    case HAND_224: {
      _score_thr = hand_detect::ESPDet::default_score_thr;
      auto *det = new (std::nothrow) HandDetect(HandDetect::ESPDET_PICO_224_224_HAND, false);
      if (!det) return false;
      _impl = det;
      _kind = KIND_WRAPPER;
      break;
    }
    default:
      ESP_LOGE(TAG, "unknown model %d", (int)model);
      return false;
  }

  ESP_LOGI(TAG, "ready model=%s thr=%.2f", modelName(model), (double)_score_thr);
  return true;
}

void ESP32P4_ObjectDetect::end() {
  if (_yolo26) {
    delete static_cast<YOLO26 *>(_yolo26);
    _yolo26 = nullptr;
  }
  if (_dl_model) {
    delete static_cast<dl::Model *>(_dl_model);
    _dl_model = nullptr;
  }
  if (_impl) {
    switch (_model) {
      case COCO_YOLO11N:
      case COCO_YOLO11N_320:
        delete static_cast<COCODetect *>(_impl);
        break;
      case PEDESTRIAN_PICO:
        delete static_cast<PedestrianDetect *>(_impl);
        break;
      case CAT_224:
      case CAT_416:
        delete static_cast<CatDetect *>(_impl);
        break;
      case DOG_224:
      case DOG_416:
        delete static_cast<DogDetect *>(_impl);
        break;
      case HAND_224:
        delete static_cast<HandDetect *>(_impl);
        break;
      default:
        break;
    }
    _impl = nullptr;
  }
  _kind = KIND_NONE;
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  _last_ms = 0;
  _last_n = 0;
}

bool ESP32P4_ObjectDetect::setModel(Model m) {
  if (ready() && m == _model) return true;
  return begin(m);
}

void ESP32P4_ObjectDetect::setScoreThr(float thr) {
  if (thr < 0.01f) thr = 0.01f;
  if (thr > 0.99f) thr = 0.99f;
  _score_thr = thr;
  if (_kind == KIND_WRAPPER && _impl) {
    static_cast<dl::detect::DetectWrapper *>(_impl)->set_score_thr(thr, 0);
  }
  if (_kind == KIND_YOLO26 && _yolo26) {
    static_cast<YOLO26 *>(_yolo26)->set_conf(thr);
  }
}

bool ESP32P4_ObjectDetect::ensureRgb(size_t pixels) {
  const size_t need = pixels * 3;
  if (need <= _rgb888_cap) return true;
  if (_rgb888) {
    esp32p4_psram_free(_rgb888);
    _rgb888 = nullptr;
    _rgb888_cap = 0;
  }
  _rgb888 = (uint8_t *)esp32p4_psram_alloc(need);
  if (!_rgb888) {
    ESP_LOGE(TAG, "RGB888 alloc failed (%u)", (unsigned)need);
    return false;
  }
  _rgb888_cap = need;
  return true;
}

int ESP32P4_ObjectDetect::detect(const camera_fb_t *fb, esp32p4_det_t *out, int max_out) {
  if (!fb || !fb->buf) return 0;
  return detect((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out);
}

void ESP32P4_ObjectDetect::finishDets(esp32p4_det_t *out, int n) {
  if (!out) {
    _last_n = 0;
    return;
  }
  for (int i = 0; i < n; i++) {
    const char *lab = label(_model, out[i].category);
    out[i].label = lab ? lab : "";
  }
  int c = n < kMaxResults ? n : kMaxResults;
  if (out != _last && c > 0) memcpy(_last, out, sizeof(esp32p4_det_t) * (size_t)c);
  _last_n = n;
}

int ESP32P4_ObjectDetect::runRgb888(const uint8_t *rgb888, int dw, int dh, int src_w, int src_h,
                                    esp32p4_det_t *out, int max_out) {
  if (!ready() || !rgb888 || !out || max_out <= 0 || dw <= 0 || dh <= 0) return 0;

  dl::image::img_t img = {};
  img.data = (void *)rgb888;
  img.width = dw;
  img.height = dh;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  const int64_t t0 = esp_timer_get_time();
  int n = 0;

  if (_kind == KIND_YOLO26) {
    auto *proc = static_cast<YOLO26 *>(_yolo26);
    auto *mdl = static_cast<dl::Model *>(_dl_model);
    proc->preprocess(img);
    if (auto *tin = proc->preprocessor() ? proc->preprocessor()->get_model_input() : nullptr) {
      if (tin->data) esp32p4_psram_writeback(tin->data, (size_t)tin->get_bytes());
    }
    mdl->run();
    {
      const char *outs[] = {"one2one_p3_box", "one2one_p3_cls", "one2one_p4_box",
                            "one2one_p4_cls", "one2one_p5_box", "one2one_p5_cls"};
      for (const char *nm : outs) {
        dl::TensorBase *t = mdl->get_output(nm);
        if (!t || !t->data) continue;
        size_t ne = 1;
        for (int s : t->shape) ne *= (size_t)((s > 0) ? s : 1);
        esp32p4_psram_msync(t->data, ne * t->get_dtype_bytes());
      }
    }
    auto results = proc->postprocess(mdl->get_outputs());
    std::sort(results.begin(), results.end(), dl::detect::greater_box);
    for (size_t i = 0; i < results.size() && n < max_out; i++) {
      if (results[i].box.size() < 4) continue;
      int x1 = results[i].box[0], y1 = results[i].box[1];
      int x2 = results[i].box[2], y2 = results[i].box[3];
      if (x1 < 0) x1 = 0;
      if (y1 < 0) y1 = 0;
      if (x2 > dw) x2 = dw;
      if (y2 > dh) y2 = dh;
      int bw = x2 - x1, bh = y2 - y1;
      if (bw < 8 || bh < 8) continue;
      out[n].score = results[i].score;
      out[n].category = results[i].category;
      out[n].x = x1;
      out[n].y = y1;
      out[n].w = bw;
      out[n].h = bh;
      out[n].label = nullptr;
      n++;
    }
  } else if (_kind == KIND_WRAPPER) {
    auto &results = static_cast<dl::detect::DetectWrapper *>(_impl)->run(img);
    for (const auto &res : results) {
      if (n >= max_out) break;
      if (res.box.size() < 4) continue;
      out[n].score = res.score;
      out[n].category = res.category;
      out[n].x = (int)res.box[0];
      out[n].y = (int)res.box[1];
      out[n].w = (int)(res.box[2] - res.box[0]);
      out[n].h = (int)(res.box[3] - res.box[1]);
      out[n].label = nullptr;
      if (out[n].w < 1 || out[n].h < 1) continue;
      n++;
    }
  }

  if ((dw != src_w || dh != src_h) && dw > 0 && dh > 0) {
    for (int i = 0; i < n; i++) {
      out[i].x = out[i].x * src_w / dw;
      out[i].y = out[i].y * src_h / dh;
      out[i].w = out[i].w * src_w / dw;
      out[i].h = out[i].h * src_h / dh;
    }
  }

  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  finishDets(out, n);
  return n;
}

int ESP32P4_ObjectDetect::detect(const uint16_t *rgb565, int w, int h, esp32p4_det_t *out, int max_out) {
  if (!ready() || !rgb565 || !out || max_out <= 0 || w <= 0 || h <= 0) return 0;

  const uint16_t *src = rgb565;
  int dw = w, dh = h;
  uint16_t *half = nullptr;
  if ((w > 640 || h > 640) && (w % 2 == 0) && (h % 2 == 0)) {
    dw = w / 2;
    dh = h / 2;
    half = (uint16_t *)esp32p4_psram_alloc((size_t)dw * (size_t)dh * 2u);
    if (!half) return 0;
    ESP32P4_Img::downsample2x565(rgb565, w, h, half);
    src = half;
  }

  if (!ensureRgb((size_t)dw * (size_t)dh)) {
    if (half) esp32p4_psram_free(half);
    return 0;
  }

  ESP32P4_Img::rgb565ToRgb888(src, _rgb888, (size_t)dw * (size_t)dh);
  esp32p4_psram_writeback(_rgb888, (size_t)dw * (size_t)dh * 3u);
  if (half) {
    esp32p4_psram_free(half);
    half = nullptr;
  }
  return runRgb888(_rgb888, dw, dh, w, h, out, max_out);
}

int ESP32P4_ObjectDetect::detectRgb888(const uint8_t *rgb888, int w, int h, esp32p4_det_t *out,
                                       int max_out) {
  if (!ready() || !rgb888 || !out || max_out <= 0 || w <= 0 || h <= 0) return 0;
  esp32p4_psram_writeback((void *)rgb888, (size_t)w * (size_t)h * 3u);
  return runRgb888(rgb888, w, h, w, h, out, max_out);
}

int ESP32P4_ObjectDetect::detectJpeg(ESP32P4_Jpeg &jpeg, const uint8_t *jpg, size_t jpg_len,
                                     esp32p4_det_t *out, int max_out) {
  if (!jpg || !jpg_len || !out || max_out <= 0) return 0;
  uint32_t jw = 0, jh = 0;
  if (!jpeg.decodeInfo(jpg, jpg_len, &jw, &jh) || jw < 8 || jh < 8) return 0;
  const size_t need = (size_t)jw * (size_t)jh * 2u;
  uint16_t *rgb = (uint16_t *)esp32p4_psram_alloc(need);
  if (!rgb) return 0;
  size_t got = jpeg.decode(jpg, jpg_len, (uint8_t *)rgb, need, &jw, &jh);
  int n = 0;
  if (got) n = detect(rgb, (int)jw, (int)jh, out, max_out);
  esp32p4_psram_free(rgb);
  return n;
}

int ESP32P4_ObjectDetect::infer(const uint16_t *rgb565, int w, int h) {
  return detect(rgb565, w, h, _last, kMaxResults);
}

int ESP32P4_ObjectDetect::infer(const camera_fb_t *fb) {
  return detect(fb, _last, kMaxResults);
}

int ESP32P4_ObjectDetect::inferRgb888(const uint8_t *rgb888, int w, int h) {
  return detectRgb888(rgb888, w, h, _last, kMaxResults);
}

int ESP32P4_ObjectDetect::inferJpeg(ESP32P4_Jpeg &jpeg, const uint8_t *jpg, size_t jpg_len) {
  return detectJpeg(jpeg, jpg, jpg_len, _last, kMaxResults);
}

size_t ESP32P4_ObjectDetect::resultsJson(char *buf, size_t cap) const {
  int m = _last_n < kMaxResults ? _last_n : kMaxResults;
  return ESP32P4_VisionAi::detsToJson(_last, m, buf, cap, _last_ms);
}

size_t ESP32P4_ObjectDetect::toJson(const esp32p4_det_t *dets, int n, char *buf, size_t cap, int ms) {
  return ESP32P4_VisionAi::detsToJson(dets, n, buf, cap, ms);
}

void ESP32P4_ObjectDetect::draw(uint16_t *rgb565, int w, int h, const esp32p4_det_t *dets, int n,
                                Model model, uint16_t color) {
  if (!rgb565 || !dets || n <= 0 || w < 8 || h < 8) return;
  const uint16_t col_plate = 0x0841;
  const uint16_t col_text = 0xEF5D;

  for (int i = 0; i < n; i++) {
    int x = dets[i].x, y = dets[i].y, bw = dets[i].w, bh = dets[i].h;
    if (bw < 4 || bh < 4) continue;
    int x2 = x + bw, y2 = y + bh;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > w) x2 = w;
    if (y2 > h) y2 = h;
    bw = x2 - x;
    bh = y2 - y;
    /* Skip boxes that mostly live outside the visible frame (stale / bad letterbox). */
    if (bw < 8 || bh < 8) continue;
    if (bw * bh < (dets[i].w * dets[i].h) / 3) continue;
    /* Keep labels/boxes fully inside the bitmap so they cannot look "off canvas". */
    if (x <= 0 && y <= 0 && x2 >= w && y2 >= h) continue;

    esp32p4_rect_t r{x, y, bw, bh};
    ESP32P4_Img::fillRect565(rgb565, w, h, r, color, 2);

    char buf[40];
    int pct = (int)(dets[i].score * 100.f + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 99) pct = 99;
    snprintf(buf, sizeof(buf), "%s %d", label(model, dets[i].category), pct);

    const int scale = 1;
    const int tw = (int)strlen(buf) * 6 * scale;
    const int th = 7 * scale + 4;
    int lx = x;
    int ly = y - th - 2;
    if (ly < 0) ly = y + 2;
    if (ly + th > h) ly = h - th;
    if (lx + tw + 4 > w) lx = w - tw - 4;
    if (lx < 0) lx = 0;
    if (ly < 0) ly = 0;
    for (int yy = ly; yy < ly + th && yy < h; yy++) {
      for (int xx = lx; xx < lx + tw + 4 && xx < w; xx++) {
        rgb565[yy * w + xx] = col_plate;
      }
    }
    ESP32P4_Cv::putText(rgb565, w, h, lx + 2, ly + 2, buf, col_text, scale);
  }
}
