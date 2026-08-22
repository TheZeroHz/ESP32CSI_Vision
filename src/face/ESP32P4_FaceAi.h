#pragma once

/**
 * ESP-DL human face detect + recognition (vendored under src/espdl/).
 * Feature vectors persist in face.db; display names in a sidecar text file.
 *
 * Enroll / recognize follow the classic ESP32-CAM access-control pattern:
 * modes are mutually exclusive; enroll confirms N frames then writes ONE feature;
 * recognition only runs when not enrolling.
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "ESP32P4_FaceDetect.h"

#ifndef ESP32P4_FACE_MAX_NAMES
#define ESP32P4_FACE_MAX_NAMES 48
#endif

#ifndef ESP32P4_FACE_ENROLL_SAMPLES
#define ESP32P4_FACE_ENROLL_SAMPLES 5
#endif

struct esp32p4_face_id_t {
  esp32p4_face_t face;
  int id;  // enrolled id, or -1 if unknown / detect-only
  float similarity;
  char name[24];
};

class ESP32P4_FaceAi {
 public:
  using DetModel = ESP32P4_FaceDetect::Model;
  enum FeatModel : int {
    MFN_S8_V1 = 0,  // MobileFaceNet — default, ~1.3 MB
    MBF_S8_V1 = 1,  // MobileFaceNet-Big — higher TAR, ~3.4 MB
  };

  ESP32P4_FaceAi() = default;
  ~ESP32P4_FaceAi() { end(); }

  /**
   * @param db_path   feature DB path, e.g. "/sdcard/face.db" (nullptr = detect-only)
   * @param names_path sidecar names file, e.g. "/sdcard/face_names.txt"
   * @param feat      MFN (default) or MBF. Switching feat invalidates an existing DB.
   */
  bool begin(DetModel det = ESP32P4_FaceDetect::MSRMNP_S8_V1, const char *db_path = "/sdcard/face.db",
             const char *names_path = "/sdcard/face_names.txt", FeatModel feat = MFN_S8_V1);
  void end();
  bool ready() const { return _det != nullptr; }
  bool recognitionReady() const { return _rec != nullptr; }

  bool setDetModel(DetModel m);
  bool setFeatModel(FeatModel f);

  /**
   * Detect (+ optional recognize). While enroll is pending, recognize is forced off
   * (robotzero-style mutual exclusion).
   */
  int run(const uint16_t *rgb565, int w, int h, esp32p4_face_id_t *out, int max_out,
          bool recognize = true);

  int run(const camera_fb_t *fb, esp32p4_face_id_t *out, int max_out, bool recognize = true);

  /** Start multi-sample enroll (keeps trying until samples done or cancel/timeout). */
  void requestEnroll(const char *name);
  void cancelEnroll();
  bool enrollPending() const { return _enroll_req; }
  int enrollSamplesDone() const { return _enroll_got; }
  int enrollSamplesNeed() const { return _enroll_need; }

  int enroll(const uint16_t *rgb565, int w, int h, const char *name);

  bool clearDb();
  bool deleteId(uint16_t id);
  /** Delete every enrolled feature sharing this display name. */
  bool deleteName(const char *name);
  bool setName(uint16_t id, const char *name);
  const char *nameOf(int id) const;

  /**
   * Roster grouped by name: "Name#id#count|Name#id#count"
   * (one row per person; count = feature samples kept for that name).
   */
  int rosterText(char *buf, size_t buf_len) const;

  void setThresh(float thr);
  float thresh() const;

  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_face_id_t *faces, int n);

  DetModel detModel() const { return _det_model; }
  FeatModel featModel() const { return _feat_model; }
  static const char *featName(FeatModel f) {
    return f == MBF_S8_V1 ? "MBF_S8_V1" : "MFN_S8_V1";
  }
  const char *dbPath() const { return _db_path; }
  int lastMs() const { return _last_ms; }
  int lastCount() const { return _last_n; }
  int featCount() const;
  int lastEnrollId() const { return _last_enroll_id; }
  /**
   * 1=ok, 0=idle, 2=in progress (waiting for samples),
   * -1=fail/timeout, -2=no recognizer, -3=need MSR+MNP landmarks
   */
  int lastEnrollStatus() const { return _last_enroll_status; }

 private:
  struct NameEntry {
    uint16_t id = 0;
    char name[24] = {};
  };

  bool ensureRgb(size_t pixels);
  bool prepareInfer(const uint16_t *rgb565, int w, int h);
  void inferSize(int *iw, int *ih) const;
  void mapBoxToFull(esp32p4_face_id_t &f) const;
  int runImg(esp32p4_face_id_t *out, int max_out, bool recognize, bool do_enroll);
  void loadNames();
  void saveNames() const;
  void clearNames();
  void setNameLocked(uint16_t id, const char *name);
  void removeNameLocked(uint16_t id);
  static void sanitizeName(char *dst, size_t dst_len, const char *src);

  void *_det = nullptr;
  void *_rec = nullptr;
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  uint16_t *_crop565 = nullptr;
  size_t _crop565_cap = 0;
  uint16_t *_infer565 = nullptr;
  size_t _infer565_cap = 0;
  int _img_w = 0, _img_h = 0;       // model input size
  int _full_w = 0, _full_h = 0;     // original frame
  int _crop_x = 0, _crop_y = 0, _crop_w = 0, _crop_h = 0;
  DetModel _det_model = ESP32P4_FaceDetect::MSRMNP_S8_V1;
  FeatModel _feat_model = MFN_S8_V1;
  char _db_path[64] = {};
  char _names_path[64] = {};
  char _pending_name[24] = {};
  NameEntry _names[ESP32P4_FACE_MAX_NAMES];
  int _name_n = 0;
  volatile bool _enroll_req = false;
  volatile int _enroll_got = 0;
  volatile int _enroll_need = ESP32P4_FACE_ENROLL_SAMPLES;
  volatile uint32_t _enroll_deadline_ms = 0;
  volatile uint32_t _enroll_last_sample_ms = 0;
  volatile int _last_ms = 0;
  volatile int _last_n = 0;
  volatile int _last_enroll_id = 0;
  volatile int _last_enroll_status = 0;
};
