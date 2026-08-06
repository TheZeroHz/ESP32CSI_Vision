#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "cam/ESP32P4_Camera.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "ppa/ESP32P4_Ppa.h"
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

 private:
  void handleRoot();
  void handleJpg();
  void handleStream();
  void handleStatus();
  void handleControl();
  void handleCapture();

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

  bool applyControl(const String &var, int val);

  ESP32P4_Camera *_cam = nullptr;
  WebServer *_http = nullptr;         // UI + control + /jpg (never blocks)
  WebServer *_stream_http = nullptr;  // /stream only (may block)
  ESP32P4_Jpeg _jpeg;
  ESP32P4_Ppa _ppa;
  uint8_t *_jpg_buf[2] = {nullptr, nullptr};
  uint8_t *_scale_buf = nullptr;
  size_t _jpg_cap = 0;
  size_t _scale_cap = 0;
  volatile size_t _jpg_len[2] = {0, 0};
  volatile int _ready_idx = -1;
  volatile int _enc_idx = 0;
  volatile uint32_t _frame_seq = 0;
  volatile bool _worker_run = false;
  volatile bool _http_run = false;
  TaskHandle_t _worker = nullptr;
  TaskHandle_t _control_task = nullptr;
  TaskHandle_t _stream_task = nullptr;
  SemaphoreHandle_t _frame_sem = nullptr;
  SemaphoreHandle_t _jpg_mutex = nullptr;

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
};
