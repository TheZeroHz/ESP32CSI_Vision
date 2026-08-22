#include "uvc/ESP32P4_Uvc.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mem/ESP32P4_Psram.h"
#include "sdkconfig.h"

#if defined(CONFIG_TINYUSB_ENABLED) && CONFIG_TINYUSB_ENABLED && defined(CONFIG_TINYUSB_VIDEO_ENABLED) && \
    CONFIG_TINYUSB_VIDEO_ENABLED
#define ESP32P4_UVC_TINYUSB 1
#include "USB.h"
#include "class/video/video.h"
#include "esp32-hal-tinyusb.h"
#include "tusb.h"
#endif

static ESP32P4_Uvc *s_uvc = nullptr;

#if ESP32P4_UVC_TINYUSB

/* Time stamp base clock (UVC 1.5, deprecated). */
#ifndef UVC_CLOCK_FREQUENCY
#define UVC_CLOCK_FREQUENCY 27000000
#endif
#ifndef UVC_ENTITY_CAP_INPUT_TERMINAL
#define UVC_ENTITY_CAP_INPUT_TERMINAL 0x01
#endif
#ifndef UVC_ENTITY_CAP_OUTPUT_TERMINAL
#define UVC_ENTITY_CAP_OUTPUT_TERMINAL 0x02
#endif

/*
 * MJPEG bulk capture descriptor (TinyUSB / Espressif usb_device_uvc, MIT).
 * Bulk so we do not need an isochronous alt-setting in Arduino's composite
 * descriptor builder.
 */
#define TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN                                          \
  (TUD_VIDEO_DESC_IAD_LEN + TUD_VIDEO_DESC_STD_VC_LEN + (TUD_VIDEO_DESC_CS_VC_LEN + 1) + \
   TUD_VIDEO_DESC_CAMERA_TERM_LEN + TUD_VIDEO_DESC_OUTPUT_TERM_LEN + TUD_VIDEO_DESC_STD_VS_LEN + \
   (TUD_VIDEO_DESC_CS_VS_IN_LEN + 1) + TUD_VIDEO_DESC_CS_VS_FMT_MJPEG_LEN +             \
   TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_LEN + TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING_LEN + 7)

#define TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG_BULK(_stridx, _itf, _epin, _width, _height, _fps, _epsize)     \
  TUD_VIDEO_DESC_IAD(_itf, 0x02, _stridx), TUD_VIDEO_DESC_STD_VC(_itf, 0, _stridx),                        \
      TUD_VIDEO_DESC_CS_VC(0x0150, TUD_VIDEO_DESC_CAMERA_TERM_LEN + TUD_VIDEO_DESC_OUTPUT_TERM_LEN,        \
                           UVC_CLOCK_FREQUENCY, _itf + 1),                                               \
      TUD_VIDEO_DESC_CAMERA_TERM(UVC_ENTITY_CAP_INPUT_TERMINAL, 0, 0, 0, 0, 0, 0),                         \
      TUD_VIDEO_DESC_OUTPUT_TERM(UVC_ENTITY_CAP_OUTPUT_TERMINAL, VIDEO_TT_STREAMING, 0, 1, 0),             \
      TUD_VIDEO_DESC_STD_VS(_itf + 1, 0, 1, _stridx),                                                     \
      TUD_VIDEO_DESC_CS_VS_INPUT(1,                                                                       \
                                 TUD_VIDEO_DESC_CS_VS_FMT_MJPEG_LEN + TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_LEN + \
                                     TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING_LEN,                             \
                                 _epin, 0, UVC_ENTITY_CAP_OUTPUT_TERMINAL, 0, 0, 0, 0),                    \
      TUD_VIDEO_DESC_CS_VS_FMT_MJPEG(1, 1, 0, 1, 0, 0, 0, 0),                                             \
      TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT(1, 0, _width, _height, (uint32_t)_width * _height * 16,          \
                                          (uint32_t)_width * _height * 16 * (_fps),                       \
                                          (uint32_t)_width * _height * 16 / 8, (10000000 / (_fps)),        \
                                          (10000000 / (_fps)), (10000000 / (_fps)), (10000000 / (_fps))),  \
      TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING(VIDEO_COLOR_PRIMARIES_BT709, VIDEO_COLOR_XFER_CH_BT709,          \
                                          VIDEO_COLOR_COEF_SMPTE170M),                                    \
      TUD_VIDEO_DESC_EP_BULK(_epin, _epsize, 1)

extern "C" uint16_t esp32p4_uvc_load_descriptor(uint8_t *dst, uint8_t *itf) {
  uint8_t str_index = tinyusb_add_string_descriptor("CSI UVC");
  uint8_t ep_num = tinyusb_get_free_in_endpoint();
  if (!ep_num) return 0;
  uint8_t ep_in = (uint8_t)(0x80 | ep_num);
#ifdef CFG_TUD_ENDPOINT_SIZE
  uint16_t ep_size = CFG_TUD_ENDPOINT_SIZE;
#else
  uint16_t ep_size = 512;
#endif
  uint16_t w = ESP32P4_UVC_WIDTH;
  uint16_t h = ESP32P4_UVC_HEIGHT;
  uint8_t fps = ESP32P4_UVC_FPS ? ESP32P4_UVC_FPS : 15;
  if (s_uvc && s_uvc->_cam && s_uvc->_cam->width() && s_uvc->_cam->height()) {
    w = s_uvc->_cam->width();
    h = s_uvc->_cam->height();
  }
  if (s_uvc) {
    s_uvc->_w = w;
    s_uvc->_h = h;
    s_uvc->_fps = fps;
    s_uvc->_interval_ms = 1000 / fps;
  }

  uint8_t descriptor[TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN] = {
      TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG_BULK(str_index, *itf, ep_in, w, h, fps, ep_size)};
  *itf = (uint8_t)(*itf + 2);
  memcpy(dst, descriptor, sizeof(descriptor));
  return sizeof(descriptor);
}

void esp32p4_uvc_on_xfer_done() {
  if (s_uvc && s_uvc->_task) xTaskNotifyGive(s_uvc->_task);
}

int esp32p4_uvc_on_commit(uint32_t interval_100ns) {
  uint32_t ms = interval_100ns / 10000;
  if (ms < 15) ms = 15;
  if (ms > 1000) ms = 1000;
  if (s_uvc) s_uvc->_interval_ms = ms;
  return VIDEO_ERROR_NONE;
}

extern "C" void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx) {
  (void)ctl_idx;
  (void)stm_idx;
  esp32p4_uvc_on_xfer_done();
}

extern "C" int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                                   video_probe_and_commit_control_t const *parameters) {
  (void)ctl_idx;
  (void)stm_idx;
  if (!parameters) return VIDEO_ERROR_NONE;
  return esp32p4_uvc_on_commit(parameters->dwFrameInterval);
}

#else /* !ESP32P4_UVC_TINYUSB */

extern "C" uint16_t esp32p4_uvc_load_descriptor(uint8_t *dst, uint8_t *itf) {
  (void)dst;
  (void)itf;
  return 0;
}

#endif /* ESP32P4_UVC_TINYUSB */

ESP32P4_Uvc::ESP32P4_Uvc() {
  s_uvc = this;
#if ESP32P4_UVC_TINYUSB
  tinyusb_enable_interface(USB_INTERFACE_CUSTOM, TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN,
                           esp32p4_uvc_load_descriptor);
#endif
}

ESP32P4_Uvc::~ESP32P4_Uvc() {
  end();
  if (s_uvc == this) s_uvc = nullptr;
}

bool ESP32P4_Uvc::encode_frame(camera_fb_t *fb, size_t *out_len) {
  if (!fb || !_xfer || !_xfer_cap) return false;
  size_t n = 0;
  if (fb->format == ESP32P4_PIXFORMAT_JPEG) {
    if (fb->len == 0 || fb->len > _xfer_cap) return false;
    memcpy(_xfer, fb->buf, fb->len);
    n = fb->len;
  } else {
    n = _jpeg.encode(fb, _xfer, _xfer_cap);
  }
  if (n < 4) return false;
  *out_len = n;
  return true;
}

void ESP32P4_Uvc::worker(void *arg) {
  ESP32P4_Uvc *self = static_cast<ESP32P4_Uvc *>(arg);
  uint32_t start_ms = millis();
  while (self->_run) {
#if ESP32P4_UVC_TINYUSB
    if (!tud_video_n_streaming(0, 0)) {
      vTaskDelay(pdMS_TO_TICKS(10));
      start_ms = millis();
      continue;
    }
    uint32_t now = millis();
    if ((uint32_t)(now - start_ms) < self->_interval_ms) {
      vTaskDelay(1);
      continue;
    }
    start_ms += self->_interval_ms;

    camera_fb_t *fb = self->_cam ? self->_cam->capture(400) : nullptr;
    if (!fb) continue;
    size_t n = 0;
    bool ok = self->encode_frame(fb, &n);
    self->_cam->release(fb);
    if (!ok) continue;
    if (!tud_video_n_frame_xfer(0, 0, self->_xfer, n)) continue;
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0) continue;
    self->_frames++;
#else
    vTaskDelay(pdMS_TO_TICKS(100));
#endif
  }
  self->_task = nullptr;
  vTaskDelete(nullptr);
}

bool ESP32P4_Uvc::begin(ESP32P4_Camera *cam, uint8_t quality) {
  if (_started) return true;
  if (!cam) return false;
#if !ESP32P4_UVC_TINYUSB
  Serial.println("UVC: TinyUSB VIDEO is off in this core build");
  return false;
#else
  _cam = cam;
  _quality = quality < 1 ? 1 : (quality > 100 ? quality : quality);
  if (cam->width() && cam->height()) {
    _w = cam->width();
    _h = cam->height();
  }
  if (!_jpeg.begin(_w, _h, _quality)) {
    Serial.println("UVC: JPEG encoder failed");
    return false;
  }
  _xfer_cap = (size_t)_w * (size_t)_h;
  if (_xfer_cap < 64 * 1024) _xfer_cap = 64 * 1024;
  _xfer = (uint8_t *)esp32p4_psram_alloc(_xfer_cap);
  if (!_xfer) {
    Serial.println("UVC: PSRAM xfer alloc failed");
    _jpeg.end();
    return false;
  }

  USB.productName("ESP32CSI Vision");
  USB.manufacturerName("Rakib Hasan");
  if (!USB.begin()) {
    Serial.println("UVC: USB.begin failed (need USB-OTG / TinyUSB)");
    esp32p4_psram_free(_xfer);
    _xfer = nullptr;
    _jpeg.end();
    return false;
  }

  _run = true;
  _frames = 0;
  BaseType_t ok = xTaskCreatePinnedToCore(worker, "UVC", 6144, this, 5, &_task, tskNO_AFFINITY);
  if (ok != pdPASS) {
    _run = false;
    Serial.println("UVC: task create failed");
    esp32p4_psram_free(_xfer);
    _xfer = nullptr;
    _jpeg.end();
    return false;
  }
  _started = true;
  Serial.printf("UVC: MJPEG gadget %ux%u q=%u  (PC webcam on native USB-C)\n", _w, _h, _quality);
  return true;
#endif
}

void ESP32P4_Uvc::end() {
  _run = false;
  if (_task) {
    xTaskNotifyGive(_task);
    for (int i = 0; i < 50 && _task; i++) vTaskDelay(pdMS_TO_TICKS(10));
  }
  _started = false;
  _cam = nullptr;
  if (_xfer) {
    esp32p4_psram_free(_xfer);
    _xfer = nullptr;
  }
  _xfer_cap = 0;
  _jpeg.end();
}

bool ESP32P4_Uvc::streaming() const {
#if ESP32P4_UVC_TINYUSB
  return _started && tud_video_n_streaming(0, 0);
#else
  return false;
#endif
}

void ESP32P4_Uvc::setQuality(uint8_t q) {
  _quality = q < 1 ? 1 : (q > 100 ? q : q);
  _jpeg.setQuality(_quality);
}
