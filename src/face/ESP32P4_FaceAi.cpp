#include "face/ESP32P4_FaceAi.h"

#include <ctype.h>
#include <list>
#include <new>
#include <stdio.h>
#include <string.h>
#include <string>
#include <strings.h>

#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include "img/ESP32P4_Img.h"
#include "mem/ESP32P4_Psram.h"
#include "cv/ESP32P4_Cv.h"
#include "ppa/ESP32P4_Ppa.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ESP32P4_FaceAi";

static HumanFaceDetect::model_type_t toNative(ESP32P4_FaceDetect::Model m) {
  switch (m) {
    case ESP32P4_FaceDetect::ESPDET_PICO_224:
      return HumanFaceDetect::ESPDET_PICO_224_224_FACE;
    case ESP32P4_FaceDetect::ESPDET_PICO_416:
      return HumanFaceDetect::ESPDET_PICO_416_416_FACE;
    case ESP32P4_FaceDetect::MSRMNP_S8_V1:
    default:
      return HumanFaceDetect::MSRMNP_S8_V1;
  }
}

void ESP32P4_FaceAi::inferSize(int *iw, int *ih) const {
  // Working frame sizes (what we feed HumanFaceDetect), matching classic Arduino
  // CameraWebServer / esp-face practice — NOT the raw MSR net tensor size.
  //   Old esp-face (Arduino 2.0.x): detect on QVGA 320×240, align FR to 56×56
  //   ESP-DL today: MSR preprocessor resizes to 120×160; MFN aligns to 112×112
  switch (_det_model) {
    case ESP32P4_FaceDetect::ESPDET_PICO_224:
      *iw = *ih = 224;
      break;
    case ESP32P4_FaceDetect::ESPDET_PICO_416:
      *iw = *ih = 416;
      break;
    case ESP32P4_FaceDetect::MSRMNP_S8_V1:
    default:
      *iw = 320;  // QVGA — same as Arduino-ESP32 2.0.14 face examples
      *ih = 240;
      break;
  }
}

void ESP32P4_FaceAi::sanitizeName(char *dst, size_t dst_len, const char *src) {
  if (!dst || dst_len == 0) return;
  dst[0] = '\0';
  if (!src) return;
  size_t o = 0;
  for (const char *p = src; *p && o + 1 < dst_len; ++p) {
    char c = *p;
    if (isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-' || c == '.') {
      dst[o++] = c;
    }
  }
  while (o > 0 && dst[o - 1] == ' ') o--;
  dst[o] = '\0';
  if (o == 0) snprintf(dst, dst_len, "face");
}

void ESP32P4_FaceAi::clearNames() {
  _name_n = 0;
  memset(_names, 0, sizeof(_names));
}

void ESP32P4_FaceAi::loadNames() {
  clearNames();
  if (!_names_path[0]) return;
  FILE *f = fopen(_names_path, "r");
  if (!f) return;
  char line[64];
  while (fgets(line, sizeof(line), f) && _name_n < ESP32P4_FACE_MAX_NAMES) {
    unsigned id = 0;
    char name[32] = {};
    if (sscanf(line, "%u %31[^\r\n]", &id, name) >= 1 && id > 0) {
      setNameLocked((uint16_t)id, name[0] ? name : "face");
    }
  }
  fclose(f);
  ESP_LOGI(TAG, "loaded %d face names from %s", _name_n, _names_path);
}

void ESP32P4_FaceAi::saveNames() const {
  if (!_names_path[0]) return;
  FILE *f = fopen(_names_path, "w");
  if (!f) {
    ESP_LOGE(TAG, "cannot write %s", _names_path);
    return;
  }
  for (int i = 0; i < _name_n; i++) {
    fprintf(f, "%u %s\n", (unsigned)_names[i].id, _names[i].name);
  }
  fclose(f);
}

void ESP32P4_FaceAi::setNameLocked(uint16_t id, const char *name) {
  char clean[24];
  sanitizeName(clean, sizeof(clean), name);
  for (int i = 0; i < _name_n; i++) {
    if (_names[i].id == id) {
      strncpy(_names[i].name, clean, sizeof(_names[i].name) - 1);
      _names[i].name[sizeof(_names[i].name) - 1] = '\0';
      return;
    }
  }
  if (_name_n >= ESP32P4_FACE_MAX_NAMES) return;
  _names[_name_n].id = id;
  strncpy(_names[_name_n].name, clean, sizeof(_names[_name_n].name) - 1);
  _names[_name_n].name[sizeof(_names[_name_n].name) - 1] = '\0';
  _name_n++;
}

void ESP32P4_FaceAi::removeNameLocked(uint16_t id) {
  for (int i = 0; i < _name_n; i++) {
    if (_names[i].id == id) {
      _names[i] = _names[_name_n - 1];
      _name_n--;
      return;
    }
  }
}

bool ESP32P4_FaceAi::begin(DetModel det, const char *db_path, const char *names_path) {
  end();
  _det_model = det;
  _db_path[0] = '\0';
  _names_path[0] = '\0';
  if (db_path && db_path[0]) strncpy(_db_path, db_path, sizeof(_db_path) - 1);
  if (names_path && names_path[0]) strncpy(_names_path, names_path, sizeof(_names_path) - 1);

  auto *detector = new (std::nothrow) HumanFaceDetect(toNative(det), false);
  if (!detector) {
    ESP_LOGE(TAG, "HumanFaceDetect alloc failed");
    return false;
  }
  _det = detector;

  if (_db_path[0]) {
    auto *rec = new (std::nothrow) HumanFaceRecognizer(std::string(_db_path));
    if (!rec) {
      ESP_LOGW(TAG, "HumanFaceRecognizer alloc failed — detect-only");
    } else {
      _rec = rec;
      int n = rec->get_num_feats();
      ESP_LOGI(TAG, "FR ready db=%s feats=%d", _db_path, n);
    }
  }

  loadNames();
  _last_enroll_status = 0;
  _last_enroll_id = 0;
  _enroll_req = false;
  _enroll_got = 0;
  _enroll_need = ESP32P4_FACE_ENROLL_SAMPLES;
  _enroll_deadline_ms = 0;
  int iw = 320, ih = 240;
  inferSize(&iw, &ih);
  ESP_LOGI(TAG, "ready det=%d rec=%d work_frame=%dx%d", (int)det, _rec ? 1 : 0, iw, ih);
  return true;
}

void ESP32P4_FaceAi::end() {
  if (_rec) {
    delete static_cast<HumanFaceRecognizer *>(_rec);
    _rec = nullptr;
  }
  if (_det) {
    delete static_cast<HumanFaceDetect *>(_det);
    _det = nullptr;
  }
  esp32p4_psram_free(_rgb888);
  _rgb888 = nullptr;
  _rgb888_cap = 0;
  esp32p4_psram_free(_crop565);
  _crop565 = nullptr;
  _crop565_cap = 0;
  esp32p4_psram_free(_infer565);
  _infer565 = nullptr;
  _infer565_cap = 0;
  _enroll_req = false;
}

bool ESP32P4_FaceAi::setDetModel(DetModel m) {
  if (m == _det_model && _det) return true;
  char db[64], names[64];
  strncpy(db, _db_path, sizeof(db));
  strncpy(names, _names_path, sizeof(names));
  return begin(m, db[0] ? db : nullptr, names[0] ? names : nullptr);
}

bool ESP32P4_FaceAi::ensureRgb(size_t pixels) {
  size_t need = pixels * 3;
  if (need <= _rgb888_cap && _rgb888) return true;
  esp32p4_psram_free(_rgb888);
  _rgb888 = (uint8_t *)esp32p4_psram_alloc(need);
  _rgb888_cap = _rgb888 ? need : 0;
  return _rgb888 != nullptr;
}

bool ESP32P4_FaceAi::prepareInfer(const uint16_t *rgb565, int w, int h) {
  if (!rgb565 || w < 16 || h < 16) return false;
  _full_w = w;
  _full_h = h;

  int iw = 320, ih = 240;
  inferSize(&iw, &ih);

  // Already at model size (stream forced) — no extra crop/scale.
  if (w == iw && h == ih) {
    size_t px = (size_t)iw * (size_t)ih;
    if (!ensureRgb(px)) return false;
    ESP32P4_Img::rgb565ToRgb888(rgb565, _rgb888, px);
    _img_w = iw;
    _img_h = ih;
    _crop_x = 0;
    _crop_y = 0;
    _crop_w = iw;
    _crop_h = ih;
    return true;
  }

  // Center-crop to model aspect, then scale to exact model input.
  const float tar = (float)iw / (float)ih;
  const float src_ar = (float)w / (float)h;
  if (src_ar > tar) {
    _crop_h = h;
    _crop_w = (int)((float)h * tar + 0.5f) & ~1;
    if (_crop_w < 2) _crop_w = 2;
    if (_crop_w > w) _crop_w = w & ~1;
    _crop_x = ((w - _crop_w) / 2) & ~1;
    _crop_y = 0;
  } else {
    _crop_w = w;
    _crop_h = (int)((float)w / tar + 0.5f) & ~1;
    if (_crop_h < 2) _crop_h = 2;
    if (_crop_h > h) _crop_h = h & ~1;
    _crop_x = 0;
    _crop_y = ((h - _crop_h) / 2) & ~1;
  }

  size_t crop_bytes = (size_t)_crop_w * (size_t)_crop_h * 2;
  if (crop_bytes > _crop565_cap) {
    esp32p4_psram_free(_crop565);
    _crop565 = (uint16_t *)esp32p4_psram_alloc(crop_bytes);
    _crop565_cap = _crop565 ? crop_bytes : 0;
  }
  if (!_crop565) return false;

  for (int y = 0; y < _crop_h; y++) {
    const uint16_t *srcp = rgb565 + (size_t)(_crop_y + y) * (size_t)w + (size_t)_crop_x;
    memcpy(_crop565 + (size_t)y * (size_t)_crop_w, srcp, (size_t)_crop_w * 2);
  }

  size_t infer_bytes = (size_t)iw * (size_t)ih * 2;
  if (infer_bytes > _infer565_cap) {
    esp32p4_psram_free(_infer565);
    _infer565 = (uint16_t *)esp32p4_psram_alloc(infer_bytes);
    _infer565_cap = _infer565 ? infer_bytes : 0;
  }
  if (!_infer565) return false;

  if (_crop_w == iw && _crop_h == ih) {
    memcpy(_infer565, _crop565, infer_bytes);
  } else if (!ESP32P4_Ppa::cv().scaleRgb565(_crop565, _crop_w, _crop_h, _infer565, iw, ih)) {
    for (int y = 0; y < ih; y++) {
      int sy = y * _crop_h / ih;
      for (int x = 0; x < iw; x++) {
        int sx = x * _crop_w / iw;
        _infer565[y * iw + x] = _crop565[sy * _crop_w + sx];
      }
    }
  }

  if (!ensureRgb((size_t)iw * (size_t)ih)) return false;
  ESP32P4_Img::rgb565ToRgb888(_infer565, _rgb888, (size_t)iw * (size_t)ih);
  _img_w = iw;
  _img_h = ih;
  return true;
}

void ESP32P4_FaceAi::mapBoxToFull(esp32p4_face_id_t &f) const {
  if (_img_w <= 0 || _img_h <= 0 || _crop_w <= 0 || _crop_h <= 0) return;
  auto mapx = [&](int v) -> int { return _crop_x + (int)((int64_t)v * _crop_w / _img_w); };
  auto mapy = [&](int v) -> int { return _crop_y + (int)((int64_t)v * _crop_h / _img_h); };
  int x1 = mapx(f.face.x);
  int y1 = mapy(f.face.y);
  int x2 = mapx(f.face.x + f.face.w);
  int y2 = mapy(f.face.y + f.face.h);
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > _full_w) x2 = _full_w;
  if (y2 > _full_h) y2 = _full_h;
  f.face.x = x1;
  f.face.y = y1;
  f.face.w = x2 - x1;
  f.face.h = y2 - y1;
  if (f.face.has_landmarks) {
    for (int k = 0; k < 5; k++) {
      int lx = mapx(f.face.landmarks[k][0]);
      int ly = mapy(f.face.landmarks[k][1]);
      if (lx < 0) lx = 0;
      if (ly < 0) ly = 0;
      if (lx >= _full_w) lx = _full_w - 1;
      if (ly >= _full_h) ly = _full_h - 1;
      f.face.landmarks[k][0] = lx;
      f.face.landmarks[k][1] = ly;
    }
  }
}

void ESP32P4_FaceAi::requestEnroll(const char *name) {
  sanitizeName(_pending_name, sizeof(_pending_name), name);
  _enroll_req = true;
  _enroll_got = 0;
  _enroll_need = ESP32P4_FACE_ENROLL_SAMPLES;
  _enroll_deadline_ms = (uint32_t)(esp_timer_get_time() / 1000) + 20000;
  _enroll_last_sample_ms = 0;
  _last_enroll_status = 2;
  _last_enroll_id = 0;
}

void ESP32P4_FaceAi::cancelEnroll() {
  _enroll_req = false;
  _enroll_got = 0;
  _enroll_last_sample_ms = 0;
  if (_last_enroll_status == 2) _last_enroll_status = 0;
}

int ESP32P4_FaceAi::runImg(esp32p4_face_id_t *out, int max_out, bool recognize, bool do_enroll) {
  if (!_det || !_rgb888 || !out || max_out <= 0) return 0;

  dl::image::img_t img = {};
  img.data = _rgb888;
  img.width = _img_w;
  img.height = _img_h;
  img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

  auto *detector = static_cast<HumanFaceDetect *>(_det);
  int64_t t0 = esp_timer_get_time();
  auto &dets = detector->run(img);

  // Confirm N good frames, then write ONE feature (classic esp-face style — not N DB rows).
  if (do_enroll && _enroll_req) {
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (_enroll_deadline_ms && (int32_t)(now_ms - _enroll_deadline_ms) >= 0) {
      _enroll_req = false;
      _last_enroll_status = -1;
      ESP_LOGW(TAG, "enroll timeout (%d/%d confirms)", _enroll_got, _enroll_need);
    } else if (!_rec) {
      _enroll_req = false;
      _last_enroll_status = -2;
      ESP_LOGW(TAG, "enroll failed: no recognizer");
    } else if (_det_model != ESP32P4_FaceDetect::MSRMNP_S8_V1) {
      _enroll_req = false;
      _last_enroll_status = -3;
      ESP_LOGW(TAG, "enroll needs MSR+MNP (landmarks)");
    } else if (dets.empty()) {
      _last_enroll_status = 2;
    } else {
      const dl::detect::result_t *pick = nullptr;
      for (const auto &d : dets) {
        if (d.keypoint.size() >= 10) {
          if (!pick || d.box_area() > pick->box_area()) pick = &d;
        }
      }
      if (!pick) {
        _last_enroll_status = 2;
      } else if (_enroll_last_sample_ms && (now_ms - _enroll_last_sample_ms) < 350) {
        _last_enroll_status = 2;  // pace samples (~3/s max)
      } else {
        _enroll_last_sample_ms = now_ms;
        _enroll_got++;
        ESP_LOGI(TAG, "enroll confirm %d/%d name=%s", _enroll_got, _enroll_need, _pending_name);
        if (_enroll_got < _enroll_need) {
          _last_enroll_status = 2;
        } else {
          // Final frame: single DB write → one person / one ID.
          // Re-enroll replaces prior features for the same display name.
          deleteName(_pending_name);
          std::list<dl::detect::result_t> one = {*pick};
          auto *rec = static_cast<HumanFaceRecognizer *>(_rec);
          esp_err_t e = rec->enroll(img, one);
          _enroll_req = false;
          if (e == ESP_OK) {
            uint16_t id = rec->get_last_feat_id();
            setNameLocked(id, _pending_name);
            saveNames();
            _last_enroll_id = (int)id;
            _last_enroll_status = 1;
            ESP_LOGI(TAG, "enrolled id=%u name=%s (1 feature after %d confirms)", (unsigned)id,
                     _pending_name, _enroll_need);
          } else {
            _last_enroll_status = -1;
            ESP_LOGW(TAG, "enroll commit -> %s", esp_err_to_name(e));
          }
        }
      }
    }
  }

  // Recognition only when not enrolling (mutual exclusion).
  std::vector<dl::recognition::result_t> recs;
  int best_det_i = -1;
  const bool do_recog = recognize && _rec && !dets.empty() && !_enroll_req;
  if (do_recog) {
    auto *rec = static_cast<HumanFaceRecognizer *>(_rec);
    recs = rec->recognize(img, dets);
    float best_area = -1.f;
    int i = 0;
    for (const auto &res : dets) {
      float a = (float)res.box_area();
      if (a > best_area) {
        best_area = a;
        best_det_i = i;
      }
      i++;
    }
  }

  int n = 0;
  int di = 0;
  for (const auto &res : dets) {
    if (n >= max_out) break;
    if (res.box.size() < 4) {
      di++;
      continue;
    }
    esp32p4_face_id_t &f = out[n];
    memset(&f, 0, sizeof(f));
    f.face.score = res.score;
    f.face.x = (int)res.box[0];
    f.face.y = (int)res.box[1];
    f.face.w = (int)(res.box[2] - res.box[0]);
    f.face.h = (int)(res.box[3] - res.box[1]);
    f.face.has_landmarks = false;
    if (res.keypoint.size() >= 10) {
      f.face.has_landmarks = true;
      for (int k = 0; k < 5; k++) {
        f.face.landmarks[k][0] = (int)res.keypoint[k * 2];
        f.face.landmarks[k][1] = (int)res.keypoint[k * 2 + 1];
      }
    }
    f.id = -1;
    f.similarity = 0.f;
    f.name[0] = '\0';
    if (di == best_det_i && !recs.empty()) {
      f.id = (int)recs[0].id;
      f.similarity = recs[0].similarity;
      const char *nm = nameOf(f.id);
      if (nm) strncpy(f.name, nm, sizeof(f.name) - 1);
    }
    mapBoxToFull(f);
    n++;
    di++;
  }

  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  _last_n = n;
  return n;
}

int ESP32P4_FaceAi::run(const uint16_t *rgb565, int w, int h, esp32p4_face_id_t *out, int max_out,
                        bool recognize) {
  if (!prepareInfer(rgb565, w, h)) return 0;
  bool enroll = _enroll_req;
  // While enrolling, never recognize on the same pass.
  return runImg(out, max_out, recognize && _rec && !enroll, enroll);
}

int ESP32P4_FaceAi::run(const camera_fb_t *fb, esp32p4_face_id_t *out, int max_out, bool recognize) {
  if (!fb || !fb->buf) return 0;
  return run((const uint16_t *)fb->buf, fb->width, fb->height, out, max_out, recognize);
}

int ESP32P4_FaceAi::enroll(const uint16_t *rgb565, int w, int h, const char *name) {
  if (!_rec) {
    _last_enroll_status = -2;
    return -1;
  }
  requestEnroll(name);
  esp32p4_face_id_t tmp[4];
  run(rgb565, w, h, tmp, 4, false);
  return _last_enroll_status == 1 ? _last_enroll_id : -1;
}

bool ESP32P4_FaceAi::clearDb() {
  if (!_rec) return false;
  bool ok = static_cast<HumanFaceRecognizer *>(_rec)->clear_all_feats() == ESP_OK;
  clearNames();
  saveNames();
  return ok;
}

bool ESP32P4_FaceAi::deleteId(uint16_t id) {
  if (!_rec || id == 0) return false;
  bool ok = static_cast<HumanFaceRecognizer *>(_rec)->delete_feat(id) == ESP_OK;
  if (ok) {
    removeNameLocked(id);
    saveNames();
  }
  return ok;
}

bool ESP32P4_FaceAi::deleteName(const char *name) {
  if (!_rec || !name || !name[0]) return false;
  char clean[24];
  sanitizeName(clean, sizeof(clean), name);
  if (!clean[0]) return false;

  uint16_t ids[ESP32P4_FACE_MAX_NAMES];
  int n_ids = 0;
  for (int i = 0; i < _name_n && n_ids < ESP32P4_FACE_MAX_NAMES; i++) {
    if (strcasecmp(_names[i].name, clean) == 0) ids[n_ids++] = _names[i].id;
  }
  if (n_ids == 0) return false;

  auto *rec = static_cast<HumanFaceRecognizer *>(_rec);
  bool any = false;
  for (int i = 0; i < n_ids; i++) {
    if (rec->delete_feat(ids[i]) == ESP_OK) {
      removeNameLocked(ids[i]);
      any = true;
    }
  }
  if (any) saveNames();
  return any;
}

bool ESP32P4_FaceAi::setName(uint16_t id, const char *name) {
  if (id == 0 || !name) return false;
  setNameLocked(id, name);
  saveNames();
  return true;
}

const char *ESP32P4_FaceAi::nameOf(int id) const {
  if (id <= 0) return nullptr;
  for (int i = 0; i < _name_n; i++) {
    if ((int)_names[i].id == id) return _names[i].name;
  }
  return nullptr;
}

int ESP32P4_FaceAi::rosterText(char *buf, size_t buf_len) const {
  if (!buf || buf_len == 0) return 0;
  buf[0] = '\0';
  size_t o = 0;
  // One row per person name: "name#primaryId#count|..."
  for (int i = 0; i < _name_n; i++) {
    bool seen = false;
    for (int j = 0; j < i; j++) {
      if (strcasecmp(_names[j].name, _names[i].name) == 0) {
        seen = true;
        break;
      }
    }
    if (seen) continue;

    uint16_t primary = _names[i].id;
    int count = 0;
    for (int j = 0; j < _name_n; j++) {
      if (strcasecmp(_names[j].name, _names[i].name) != 0) continue;
      count++;
      if (_names[j].id < primary) primary = _names[j].id;
    }
    char piece[56];
    int n = snprintf(piece, sizeof(piece), "%s%s#%u#%d", o ? "|" : "", _names[i].name,
                     (unsigned)primary, count);
    if (n <= 0 || o + (size_t)n + 1 >= buf_len) break;
    memcpy(buf + o, piece, (size_t)n);
    o += (size_t)n;
    buf[o] = '\0';
  }
  return (int)o;
}

void ESP32P4_FaceAi::setThresh(float thr) {
  if (!_rec) return;
  if (thr < 0.1f) thr = 0.1f;
  if (thr > 0.95f) thr = 0.95f;
  static_cast<HumanFaceRecognizer *>(_rec)->set_thresh(thr);
}

float ESP32P4_FaceAi::thresh() const {
  if (!_rec) return 0.5f;
  return static_cast<HumanFaceRecognizer *>(_rec)->get_thresh();
}

int ESP32P4_FaceAi::featCount() const {
  if (!_rec) return 0;
  return static_cast<HumanFaceRecognizer *>(_rec)->get_num_feats();
}

static void faceFillSolid565(uint16_t *img, int w, int h, int x0, int y0, int x1, int y1,
                             uint16_t color) {
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 >= w) x1 = w - 1;
  if (y1 >= h) y1 = h - 1;
  if (x0 > x1 || y0 > y1) return;
  for (int y = y0; y <= y1; y++) {
    uint16_t *row = img + y * w + x0;
    for (int x = x0; x <= x1; x++) *row++ = color;
  }
}

static void faceDrawBracket(uint16_t *img, int w, int h, int x0, int y0, int x1, int y1,
                            int arm, uint16_t color) {
  if (arm < 6) arm = 6;
  // Top-left
  ESP32P4_Cv::line(img, w, h, x0, y0, x0 + arm, y0, color, 1);
  ESP32P4_Cv::line(img, w, h, x0, y0, x0, y0 + arm, color, 1);
  ESP32P4_Cv::line(img, w, h, x1 - arm, y0, x1, y0, color, 1);
  ESP32P4_Cv::line(img, w, h, x1, y0, x1, y0 + arm, color, 1);
  ESP32P4_Cv::line(img, w, h, x0, y1, x0 + arm, y1, color, 1);
  ESP32P4_Cv::line(img, w, h, x0, y1 - arm, x0, y1, color, 1);
  ESP32P4_Cv::line(img, w, h, x1 - arm, y1, x1, y1, color, 1);
  ESP32P4_Cv::line(img, w, h, x1, y1 - arm, x1, y1, color, 1);
}

void ESP32P4_FaceAi::draw(uint16_t *rgb565, int w, int h, const esp32p4_face_id_t *faces, int n) {
  if (!rgb565 || !faces || n <= 0 || w < 8 || h < 8) return;
  // Industrial HUD palette (RGB565): steel detect / process green match / plate black.
  const uint16_t col_det = 0x6CD6;    // #6B9BB0 steel
  const uint16_t col_ok = 0x2DCF;     // #2EB87A process green
  const uint16_t col_plate = 0x0841;  // #081018
  const uint16_t col_text = 0xEF5D;   // #EEEEEE
  const int inset = 2;

  for (int i = 0; i < n; i++) {
    const esp32p4_face_id_t &f = faces[i];
    const bool matched = f.id >= 0;
    const uint16_t col = matched ? col_ok : col_det;
    int x0 = f.face.x;
    int y0 = f.face.y;
    int x1 = f.face.x + f.face.w - 1;
    int y1 = f.face.y + f.face.h - 1;
    if (x0 < inset) x0 = inset;
    if (y0 < inset) y0 = inset;
    if (x1 > w - 1 - inset) x1 = w - 1 - inset;
    if (y1 > h - 1 - inset) y1 = h - 1 - inset;
    if (x1 <= x0 || y1 <= y0) continue;

    int arm = (x1 - x0) < (y1 - y0) ? (x1 - x0) : (y1 - y0);
    arm = arm / 5;
    if (arm > 28) arm = 28;
    if (arm > (x1 - x0) / 2) arm = (x1 - x0) / 2;
    if (arm > (y1 - y0) / 2) arm = (y1 - y0) / 2;
    faceDrawBracket(rgb565, w, h, x0, y0, x1, y1, arm, col);

    if (f.face.has_landmarks && !matched) {
      for (int k = 0; k < 5; k++) {
        int lx = f.face.landmarks[k][0];
        int ly = f.face.landmarks[k][1];
        if (lx < inset || ly < inset || lx >= w - inset || ly >= h - inset) continue;
        ESP32P4_Cv::circle(rgb565, w, h, lx, ly, 1, col, 1);
      }
    }

    char label[48];
    if (matched) {
      int pct = (int)(f.similarity * 100.f + 0.5f);
      if (pct < 0) pct = 0;
      if (pct > 99) pct = 99;
      char uname[24];
      uname[0] = '\0';
      if (f.name[0]) {
        size_t ui = 0;
        for (; f.name[ui] && ui + 1 < sizeof(uname); ui++) {
          char c = f.name[ui];
          uname[ui] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
        }
        uname[ui] = '\0';
        snprintf(label, sizeof(label), "ID %d  %s  %d%%", f.id, uname, pct);
      } else {
        snprintf(label, sizeof(label), "ID %d  MATCH  %d%%", f.id, pct);
      }
    } else {
      int pct = (int)(f.face.score * 100.f + 0.5f);
      if (pct < 0) pct = 0;
      if (pct > 99) pct = 99;
      snprintf(label, sizeof(label), "DETECT  %d%%", pct);
    }

    const int scale = 1;
    const int tw = (int)strlen(label) * 6 * scale;
    const int th = 7 * scale + 4;
    int lx = x0;
    // Prefer label inside the box top — never spill into letterbox / below frame.
    int ly = y0 + 2;
    if (ly + th > y1 - 2) ly = y0 - th - 2;
    if (ly < inset) ly = inset;
    if (ly + th > h - inset) ly = h - inset - th;
    if (ly < inset || ly + th > h - inset) continue;
    if (lx + tw + 6 > w - inset) lx = w - inset - tw - 6;
    if (lx < inset) lx = inset;
    faceFillSolid565(rgb565, w, h, lx, ly, lx + tw + 5, ly + th, col_plate);
    faceFillSolid565(rgb565, w, h, lx, ly, lx + 1, ly + th, col);
    ESP32P4_Cv::putText(rgb565, w, h, lx + 4, ly + 2, label, matched ? col_ok : col_text, scale);
  }
}
