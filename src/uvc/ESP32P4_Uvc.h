#pragma once

#include "cam/ESP32P4_Camera.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * USB Video Class (UVC) gadget — CSI → HW JPEG → PC webcam.
 *
 * Not V4L2. Not USB-host UVC. Uses Arduino-ESP32 TinyUSB VIDEO (P4 sdkconfig
 * already sets CONFIG_TINYUSB_VIDEO_ENABLED). Do not use with
 * `ESP32P4_CAM_BUS_UVC_HOST` in the same sketch — gadget and host cannot share
 * the USB PHY.
 *
 * Construct as a **global** so the UVC interface is registered before
 * USB CDC starts. Payload is MJPEG bulk JPEG — not H.264 (Arduino TinyUSB
 * VIDEO has no reliable H.264 gadget path).
 *
 *   ESP32P4_Camera cam;
 *   ESP32P4_Uvc uvc;
 *   void setup() {
 *     Serial.begin(115200);   // UART on typical P4 boards
 *     cam.begin(APP_BOARD);
 *     uvc.begin(&cam);
 *   }
 *
 * Board: USB Mode = USB-OTG (TinyUSB). Prefer **USB CDC On Boot = Disabled**
 * so advertised width/height match CSI (USB starts after cam.begin). With CDC
 * on boot, the native USB-C port is composite CDC+UVC and the descriptor
 * size falls back to ESP32P4_UVC_WIDTH/HEIGHT (default 800×640).
 */

#ifndef ESP32P4_UVC_WIDTH
#define ESP32P4_UVC_WIDTH 800
#endif
#ifndef ESP32P4_UVC_HEIGHT
#define ESP32P4_UVC_HEIGHT 640
#endif
#ifndef ESP32P4_UVC_FPS
#define ESP32P4_UVC_FPS 15
#endif

#ifdef __cplusplus
extern "C" {
#endif
uint16_t esp32p4_uvc_load_descriptor(uint8_t *dst, uint8_t *itf);
#ifdef __cplusplus
}
#endif

class ESP32P4_Uvc {
 public:
  ESP32P4_Uvc();
  ~ESP32P4_Uvc();

  bool begin(ESP32P4_Camera *cam, uint8_t quality = 45);
  void end();

  bool ready() const { return _started; }
  bool streaming() const;
  void setQuality(uint8_t q);
  uint8_t quality() const { return _quality; }
  uint32_t frames() const { return _frames; }
  uint16_t width() const { return _w; }
  uint16_t height() const { return _h; }

 private:
  friend uint16_t esp32p4_uvc_load_descriptor(uint8_t *dst, uint8_t *itf);
  friend void esp32p4_uvc_on_xfer_done();
  friend int esp32p4_uvc_on_commit(uint32_t interval_100ns);
  static void worker(void *arg);

  bool encode_frame(camera_fb_t *fb, size_t *out_len);

  ESP32P4_Camera *_cam = nullptr;
  ESP32P4_Jpeg _jpeg;
  uint8_t *_xfer = nullptr;
  size_t _xfer_cap = 0;
  TaskHandle_t _task = nullptr;
  uint16_t _w = ESP32P4_UVC_WIDTH;
  uint16_t _h = ESP32P4_UVC_HEIGHT;
  uint8_t _fps = ESP32P4_UVC_FPS;
  uint8_t _quality = 45;
  uint32_t _interval_ms = 1000 / ESP32P4_UVC_FPS;
  uint32_t _frames = 0;
  bool _started = false;
  volatile bool _run = false;
};
