#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "cam/ESP32P4_Camera.h"
#include "h264/ESP32P4_H264.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "ppa/ESP32P4_Ppa.h"
#include "sd/ESP32P4_Sd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/** Stream output size (PPA scale from native CSI, applied live). */
enum esp32p4_stream_framesize_t : uint8_t {
  ESP32P4_STREAM_SVGA = 0,   // 800x640 native
  ESP32P4_STREAM_VGA = 1,    // 640x480
  ESP32P4_STREAM_HVGA = 2,   // 480x320
  ESP32P4_STREAM_QVGA = 3,   // 320x240
  ESP32P4_STREAM_QQVGA = 4,  // 160x120
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

  /** Enable "Capture Img" → save JPEG into folder on SD (e.g. "/IMG"). Call after begin(). */
  bool enableSdCapture(ESP32P4_Sd *sd, const char *folder = "/IMG");
  void disableSdCapture();
  bool sdCaptureEnabled() const { return _sd != nullptr && _sd->mounted(); }
  uint32_t savedCount() const { return _saved; }
  const char *lastSavedPath() const { return _last_saved; }
  const char *sdFolder() const { return _sd_folder; }

  /**
   * Phone-style H.264 video → .mp4 on SD (start/stop from Web UI).
   * Call after begin(). Encodes as fast as possible; duration = wall clock.
   */
  bool enableVideoRecord(ESP32P4_Sd *sd, ESP32P4_H264 *h264, const char *folder = "/VIDEO");
  void disableVideoRecord();
  bool videoRecordEnabled() const {
    return _rec_sd != nullptr && _rec_sd->mounted() && _h264 != nullptr && _h264->ready();
  }
  bool isRecording() const { return _recording; }
  uint32_t videosSaved() const { return _videos; }
  const char *lastVideoPath() const { return _last_video; }
  const char *videoFolder() const { return _video_folder; }

 private:
  void handleRoot();
  void handleJpg();
  void handleStream();
  void handleStatus();
  void handleControl();
  void handleCapture();
  void handleCaptureImg();
  void handleRecordStart();
  void handleRecordStop();

  bool startWorker();
  void stopWorker();
  bool startHttpTasks();
  void stopHttpTasks();
  static void workerThunk(void *arg);
  static void controlHttpThunk(void *arg);
  static void streamHttpThunk(void *arg);
  void workerLoop();
  void controlHttpLoop();
  void streamHttpLoop();
  void applyFramesizeDims();
  void sendJpeg(WebServer *srv);
  bool saveReadyJpegToSd(char *path_out, size_t path_cap, size_t *bytes_out = nullptr);
  bool nextVideoPath(char *out, size_t out_cap);
  bool startVideoRecord();
  bool stopVideoRecord();

  bool applyControl(const String &var, int val);

  ESP32P4_Camera *_cam = nullptr;
  WebServer *_http = nullptr;         // UI + control + /jpg (never blocks)
  WebServer *_stream_http = nullptr;  // /stream only (may block)
  ESP32P4_Jpeg _jpeg;
  ESP32P4_Ppa _ppa;
  ESP32P4_Sd *_sd = nullptr;
  ESP32P4_Sd *_rec_sd = nullptr;
  ESP32P4_H264 *_h264 = nullptr;
  uint8_t *_jpg_buf[2] = {nullptr, nullptr};
  uint8_t *_scale_buf = nullptr;
  uint8_t *_save_buf = nullptr;
  uint8_t *_rec_scale_buf = nullptr;
  size_t _jpg_cap = 0;
  size_t _scale_cap = 0;
  size_t _save_cap = 0;
  size_t _rec_scale_cap = 0;
  volatile size_t _jpg_len[2] = {0, 0};
  volatile int _ready_idx = -1;
  volatile int _enc_idx = 0;
  volatile uint32_t _frame_seq = 0;
  volatile bool _worker_run = false;
  volatile bool _http_run = false;
  volatile bool _recording = false;
  TaskHandle_t _worker = nullptr;
  TaskHandle_t _control_task = nullptr;
  TaskHandle_t _stream_task = nullptr;
  SemaphoreHandle_t _frame_sem = nullptr;
  SemaphoreHandle_t _jpg_mutex = nullptr;
  SemaphoreHandle_t _rec_mutex = nullptr;

  uint16_t _port = 80;
  uint16_t _stream_port = 81;
  uint8_t _quality = 35;
  uint8_t _frame_skip = 0;
  volatile uint8_t _framesize = ESP32P4_STREAM_SVGA;
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
