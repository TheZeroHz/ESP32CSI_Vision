#pragma once

/**
 * Minimal MJPEG preview the sketch owns.
 *
 * You capture() → pass the camera_fb_t in → the browser shows it.
 * No hidden worker, no fat Camera UI. JSON at /dets is whatever you set.
 *
 *   preview.begin(80, 40);
 *   camera_fb_t *fb = cam.capture();
 *   det.infer(fb);
 *   preview.setStatusJson(...);
 *   preview.present(fb);
 *   cam.release(fb);
 *   preview.loop();
 */

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "cam/ESP32P4_Camera.h"
#include "jpeg/ESP32P4_Jpeg.h"

class ESP32P4_WebPreview {
 public:
  bool begin(uint16_t port = 80, uint8_t quality = 40, uint16_t max_w = 1920,
             uint16_t max_h = 1080);
  void end();
  void loop();

  /** Encode this framebuffer (RGB565/JPEG/…) and publish it to / and /stream. */
  bool present(const camera_fb_t *fb);
  bool presentRgb565(const uint16_t *rgb565, int w, int h);

  void setTitle(const char *title);
  void setStatusJson(const char *json);
  const char *statusJson() const { return _json; }

  uint16_t port() const { return _port; }
  uint32_t presented() const { return _presented; }
  uint32_t lastJpegBytes() const { return _last_jpeg; }

 private:
  void handleRoot();
  void handleJpg();
  void handleStream();
  void handleDets();
  static void httpThunk(void *arg);
  void httpLoop();

  WebServer *_http = nullptr;
  ESP32P4_Jpeg _jpeg;
  uint8_t *_jpg[2] = {nullptr, nullptr};
  size_t _jpg_cap = 0;
  volatile size_t _jpg_len[2] = {0, 0};
  volatile int _ready = -1;
  int _enc = 0;
  uint16_t _port = 80;
  uint32_t _presented = 0;
  uint32_t _last_jpeg = 0;
  char _title[48] = "ESP32CSI preview";
  char _json[1024] = "{\"n\":0}";
  SemaphoreHandle_t _mu = nullptr;
  TaskHandle_t _http_task = nullptr;
  volatile bool _http_run = false;
};
