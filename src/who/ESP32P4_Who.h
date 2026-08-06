#pragma once

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// ESP-WHO-style frame metadata (Arduino-safe, no ESP-DL dependency).
// For real face detect / model zoo, build idf_examples/08_FaceDetect with ESP-IDF
// and use ESP32P4_FaceDetect (idf_src/) which wraps espressif/human_face_detect.
struct esp32p4_who_fb_t {
  uint8_t *buf;
  size_t len;
  uint16_t width;
  uint16_t height;
  uint32_t timestamp_us;
  void *user;
};

typedef void (*esp32p4_who_cb_t)(const esp32p4_who_fb_t *fb, void *ctx);

class ESP32P4_WhoPipeline {
 public:
  bool begin(ESP32P4_Camera *cam, uint8_t queue_len = 2);
  void end();
  void onFrame(esp32p4_who_cb_t cb, void *ctx = nullptr);
  bool waitFrame(esp32p4_who_fb_t *out, uint32_t timeout_ms = 1000);
  bool running() const { return _task != nullptr; }

 private:
  static void taskThunk(void *arg);
  void taskLoop();

  ESP32P4_Camera *_cam = nullptr;
  QueueHandle_t _q = nullptr;
  TaskHandle_t _task = nullptr;
  esp32p4_who_cb_t _cb = nullptr;
  void *_cb_ctx = nullptr;
  volatile bool _run = false;
};
