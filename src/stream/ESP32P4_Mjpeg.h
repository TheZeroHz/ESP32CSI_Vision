#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "audio/ESP32P4_Mic.h"
#include "cam/ESP32P4_Camera.h"
#include "cam/ESP32P4_SmartAe.h"
#include "cv/ESP32P4_CvDash.h"
#include "h264/ESP32P4_H264.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "ppa/ESP32P4_Ppa.h"
#include "qr/ESP32P4_Qr.h"
#include "sd/ESP32P4_Sd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/** Stream output size — fixed ESP32-CAM-style list.
 *  Every size is a multiple of 16 (JPEG MCU). PPA center-crops then scales. */
enum esp32p4_stream_framesize_t : uint8_t {
  ESP32P4_STREAM_FHD = 0,    // 1920x1072
  ESP32P4_STREAM_HD = 1,     // 1280x720
  ESP32P4_STREAM_XGA = 2,    // 1024x576
  ESP32P4_STREAM_SVGA = 3,   // 800x640
  ESP32P4_STREAM_VGA = 4,    // 640x480
  ESP32P4_STREAM_HVGA = 5,   // 480x320
  ESP32P4_STREAM_CIF = 6,    // 400x288
  ESP32P4_STREAM_QVGA = 7,   // 320x240
  ESP32P4_STREAM_HQVGA = 8,  // 240x176
  ESP32P4_STREAM_QQVGA = 9,  // 160x128
  ESP32P4_STREAM_COUNT = 10,
  // compat aliases
  ESP32P4_STREAM_NATIVE = ESP32P4_STREAM_FHD,
  ESP32P4_STREAM_HALF = ESP32P4_STREAM_HD,
  ESP32P4_STREAM_720ISH = ESP32P4_STREAM_HD,
};

class ESP32P4_MjpegServer {
 public:
  bool begin(ESP32P4_Camera *cam, uint16_t port = 80, uint8_t quality = 35);
  void loop();
  void end();

  uint32_t sent() const { return _sent; }
  uint32_t lastJpegBytes() const { return _last_jpeg; }
  uint8_t quality() const { return _quality; }
  uint8_t frameSkip() const { return _frame_skip; }
  uint8_t framesize() const { return _framesize; }
  uint16_t outWidth() const { return _out_w; }
  uint16_t outHeight() const { return _out_h; }
  uint16_t controlPort() const { return _port; }
  uint16_t streamPort() const { return _stream_port; }

  void setQuality(uint8_t q);
  void setFrameSkip(uint8_t skip);
  bool setFramesize(uint8_t fs);

  /**
   * Enable "Capture Img" → save JPEG into folder (e.g. "/IMG") on any mounted FS
   * (SD_MMC, FFat, LittleFS, …). Call after begin().
   */
  bool enableCapture(fs::FS *fs, const char *folder = "/IMG");
  bool enableSdCapture(fs::FS *fs, const char *folder = "/IMG");
  bool enableSdCapture(ESP32P4_Sd *sd, const char *folder = "/IMG");
  void disableSdCapture();
  bool sdCaptureEnabled() const { return _store != nullptr; }
  uint32_t savedCount() const { return _saved; }
  const char *lastSavedPath() const { return _last_saved; }
  const char *sdFolder() const { return _sd_folder; }

  /**
   * Phone-style H.264 video → .mp4 on storage (start/stop from Web UI).
   * Call after begin(). Encodes as fast as possible; duration = wall clock.
   */
  bool enableVideoRecord(fs::FS *fs, ESP32P4_H264 *h264, const char *folder = "/VIDEO");
  bool enableVideoRecord(ESP32P4_Sd *sd, ESP32P4_H264 *h264, const char *folder = "/VIDEO");
  void disableVideoRecord();
  bool videoRecordEnabled() const {
    return _rec_fs != nullptr && _h264 != nullptr && _h264->ready();
  }
  bool isRecording() const { return _recording; }
  uint32_t videosSaved() const { return _videos; }
  const char *lastVideoPath() const { return _last_video; }
  const char *videoFolder() const { return _video_folder; }

  /** Optional ES8311 mic: live waveform + PCM fused into MP4 on Record/Stop. */
  bool enableMic(ESP32P4_Mic *mic);
  void disableMic();
  bool micEnabled() const { return _mic != nullptr && _mic->ready(); }

  /**
   * Show a "Files" link in the camera UI to another HTTP port (e.g. WebFileManager).
   * Pass 0 to hide. Call before or after begin().
   */
  void setFilesBrowserPort(uint16_t port);
  uint16_t filesBrowserPort() const { return _files_port; }

  /**
   * Optional RGB565 overlay hook (OpenCV-style annotate) before JPEG encode.
   * Runs on a mutable stream buffer (never mutates the camera FB used for H.264).
   * Keep it fast — it runs on the encode worker.
   */
  using FrameHook = void (*)(uint16_t *rgb565, int w, int h, void *user);
  void setFrameHook(FrameHook hook, void *user = nullptr);
  void clearFrameHook() { setFrameHook(nullptr, nullptr); }

  /**
   * Built-in OpenCV-style dashboard in the web UI (modes, HSV presets, morph, edges…).
   * Installs an internal frame hook (replaces any custom setFrameHook).
   */
  bool enableCvDashboard(bool on = true);
  bool cvDashboardEnabled() const { return _cv_on; }
  esp32p4_cv_dash_cfg_t &cvConfig() { return _cv; }
  const esp32p4_cv_dash_cfg_t &cvConfig() const { return _cv; }

  /**
   * Face detect/recognize web panel.
   * Sketch runs ESP32P4_FaceAi in setFrameHook and publishes status via faceUi().
   */
  struct FaceUi {
    bool on = false;
    bool detect_en = false;
    bool recog_en = false;
    int model = 0;  // 0=MSR+MNP 1=ESPDet224 2=ESPDet416
    int faces = 0;
    int ms = 0;
    int feats = 0;
    int thr_pct = 50;  // match threshold percent (10–95)
    int enroll_ok = 0; // 1 ok, 0 idle, 2 progress, -1 fail, -2 no FR, -3 need MSR
    int enroll_id = 0;
    int enroll_got = 0;
    int enroll_need = 5;
    char enroll_name[24] = {};
    char roster[384] = {};  // "name#id#count|..."
    char delete_name[24] = {};
    char db_path[48] = "/sdcard/face/face.db";
    volatile bool enroll_req = false;
    volatile bool enroll_cancel = false;
    volatile bool clear_req = false;
    volatile bool model_req = false;
    volatile bool delete_req = false;
    volatile int delete_id = 0;
    volatile bool delete_name_req = false;
    volatile bool thr_req = false;
    volatile bool settings_dirty = false;  // sketch persists to SD
  };
  bool enableFaceUi(bool on = true);
  bool faceUiEnabled() const { return _face.on; }
  FaceUi &faceUi() { return _face; }
  const FaceUi &faceUi() const { return _face; }
  /** True when detect/recog/enroll forces stream to the model input size. */
  bool faceResLocked() const {
    return _face.on && (_face.detect_en || _face.recog_en || _face.enroll_ok == 2);
  }
  /** Re-apply forced face dims after UI toggles detect/recog/enroll from the sketch. */
  void syncFaceStreamSize() {
    applyFramesizeDims();
    _size_dirty = true;
  }

  /**
   * QR scan web panel (zxing-cpp + PPA). Sketch runs ESP32P4_Qr in setFrameHook
   * and publishes results via qrUi().
   */
  struct QrUi {
    bool on = false;
    bool scan_en = false;
    int codes = 0;
    int ms = 0;
    uint32_t formats = 0;  // ZXing BarcodeFormat bits; 0 → use ESP32P4_Qr::defaultFormats()
    char payload[ESP32P4_QR_MAX_PAYLOAD] = {};
    char format_name[24] = {};
    bool settings_dirty = false;
  };
  bool enableQrUi(bool on = true);
  bool qrUiEnabled() const { return _qr.on; }
  QrUi &qrUi() { return _qr; }
  const QrUi &qrUi() const { return _qr; }

  /** Phone-like software AE (center-weighted luma → exposure then gain). */
  bool enableSmartAe(bool on = true);
  bool smartAeEnabled() const { return _smart_ae.enabled(); }
  ESP32P4_SmartAe &smartAe() { return _smart_ae; }
  const ESP32P4_SmartAe &smartAe() const { return _smart_ae; }

 private:
  void applyFaceForcedDims();
  static void faceModelToWH(int model, uint16_t *w, uint16_t *h);
  void handleRoot();
  void handleJpg();
  void handleStream();
  void handleStatus();
  void handleControl();
  void handleCapture();
  void handleCaptureImg();
  void handleRecordStart();
  void handleRecordStop();
  void handleAudio();

  bool startWorker();
  void stopWorker();
  bool startHttpTasks();
  void stopHttpTasks();
  static void workerThunk(void *arg);
  static void controlHttpThunk(void *arg);
  static void streamHttpThunk(void *arg);
  static void micThunk(void *arg);
  void workerLoop();
  void controlHttpLoop();
  void streamHttpLoop();
  void micLoop();
  bool startMicTask();
  void stopMicTask();
  void applyFramesizeDims();
  void sendJpeg(WebServer *srv);
  bool saveReadyJpegToSd(char *path_out, size_t path_cap, size_t *bytes_out = nullptr);
  bool nextVideoPath(char *out, size_t out_cap);
  bool startVideoRecord();
  bool stopVideoRecord();
  void finalizeVideoRecord();
  static void finalizeRecThunk(void *arg);

  bool applyControl(const String &var, int val);

  ESP32P4_Camera *_cam = nullptr;
  WebServer *_http = nullptr;         // UI + control + /jpg (never blocks)
  WebServer *_stream_http = nullptr;  // /stream only (may block)
  ESP32P4_Jpeg _jpeg;
  ESP32P4_Ppa _ppa;
  fs::FS *_store = nullptr;
  fs::FS *_rec_fs = nullptr;
  ESP32P4_H264 *_h264 = nullptr;
  ESP32P4_Mic *_mic = nullptr;
  uint8_t *_jpg_buf[2] = {nullptr, nullptr};
  uint8_t *_scale_buf = nullptr;
  uint8_t *_save_buf = nullptr;
  uint8_t *_rec_scale_buf = nullptr;
  size_t _jpg_cap = 0;
  size_t _scale_cap = 0;
  size_t _save_cap = 0;
  size_t _rec_scale_cap = 0;
  volatile size_t _jpg_len[2] = {0, 0};
  volatile uint8_t _jpg_busy[2] = {0, 0};  // slot held by /stream send
  volatile int _ready_idx = -1;
  volatile int _enc_idx = 0;
  volatile uint32_t _frame_seq = 0;
  volatile bool _size_dirty = false;  // clear buffers after framesize change
  volatile bool _worker_run = false;
  volatile bool _http_run = false;
  volatile bool _recording = false;
  volatile bool _rec_finalizing = false;
  TaskHandle_t _worker = nullptr;
  TaskHandle_t _control_task = nullptr;
  TaskHandle_t _stream_task = nullptr;
  TaskHandle_t _mic_task = nullptr;
  TaskHandle_t _rec_finalize_task = nullptr;
  volatile bool _mic_task_run = false;
  SemaphoreHandle_t _frame_sem = nullptr;
  SemaphoreHandle_t _jpg_mutex = nullptr;
  SemaphoreHandle_t _rec_mutex = nullptr;

  FrameHook _frame_hook = nullptr;
  void *_frame_hook_user = nullptr;
  bool _cv_on = false;
  esp32p4_cv_dash_cfg_t _cv{};
  FaceUi _face{};
  QrUi _qr{};
  ESP32P4_SmartAe _smart_ae{};

  static void cvDashHook(uint16_t *rgb, int w, int h, void *user);

  uint16_t _port = 80;
  uint16_t _stream_port = 81;
  uint16_t _files_port = 0;
  uint8_t _quality = 35;
  uint8_t _frame_skip = 0;
  volatile uint8_t _framesize = ESP32P4_STREAM_HD;
  volatile uint16_t _out_w = 800;
  volatile uint16_t _out_h = 640;
  uint32_t _sent = 0;
  uint32_t _last_jpeg = 0;
  uint32_t _dropped = 0;
  uint32_t _encode_ms = 0;
  uint32_t _saved = 0;
  uint32_t _save_index = 0;
  uint32_t _videos = 0;
  uint32_t _video_index = 0;
  char _sd_folder[32] = "/IMG";
  char _video_folder[32] = "/VIDEO";
  char _last_saved[64] = "";
  char _last_video[64] = "";
};
