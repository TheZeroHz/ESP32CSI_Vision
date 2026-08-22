#pragma once

/**
 * ESP-DL human face detect wrapper (ESP-IDF builds only).
 *
 * Arduino IDE does not ship esp-dl / human_face_detect. Build the
 * idf_examples/08_FaceDetect project with ESP-IDF 5.4/5.5 instead:
 *   idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
 *   idf.py build flash monitor
 */

#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"

enum esp32p4_face_det_model_t : int {
  ESP32P4_FACE_MSRMNP_S8_V1 = 0,
  ESP32P4_FACE_ESPDET_PICO_224 = 1,
  ESP32P4_FACE_ESPDET_PICO_416 = 2,
};

struct esp32p4_face_t {
  float score;
  int x;
  int y;
  int w;
  int h;
  int landmarks[5][2];  // left_eye, left_mouth, nose, right_eye, right_mouth
  bool has_landmarks;
};

class ESP32P4_FaceDetect {
 public:
  enum Model : int {
    MSRMNP_S8_V1 = ESP32P4_FACE_MSRMNP_S8_V1,
    ESPDET_PICO_224 = ESP32P4_FACE_ESPDET_PICO_224,
    ESPDET_PICO_416 = ESP32P4_FACE_ESPDET_PICO_416,
  };

  ESP32P4_FaceDetect() = default;
  ~ESP32P4_FaceDetect() { end(); }

  /** Load ESP-DL HumanFaceDetect model (P4 weights from flash rodata by default). */
  bool begin(Model model = MSRMNP_S8_V1);
  void end();

  bool ready() const { return _impl != nullptr; }

  /**
   * Run detection on an RGB565 camera framebuffer.
   * @return number of faces written to out (capped by max_out).
   */
  int detect(const camera_fb_t *fb, esp32p4_face_t *out, int max_out);

  /** Same as detect(), but on a raw RGB565 buffer (e.g. MJPEG stream scale buffer). */
  int detect(const uint16_t *rgb565, int w, int h, esp32p4_face_t *out, int max_out);

  /** Draw face boxes + landmarks on RGB565. */
  static void draw(uint16_t *rgb565, int w, int h, const esp32p4_face_t *faces, int n,
                   uint16_t color = 0x07FF);

 private:
  void *_impl = nullptr;  // HumanFaceDetect*
  uint8_t *_rgb888 = nullptr;
  size_t _rgb888_cap = 0;
  Model _model = MSRMNP_S8_V1;
};
