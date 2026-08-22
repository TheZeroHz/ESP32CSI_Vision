#include "cam/ESP32P4_Camera.h"

#include <Wire.h>
#include <string.h>

#include "cam/ESP32P4_Isp.h"
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "driver/isp.h"
#include "driver/isp_demosaic.h"
#include "driver/ledc.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_ctlr_dvp.h"
#include "driver/spi_master.h"
#include "esp_cam_sensor_types.h"
#include "hal/cam_ctlr_types.h"
#include "esp_ldo_regulator.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal/color_types.h"
#include "hal/isp_ll.h"
#include "mem/ESP32P4_Psram.h"
#include "soc/gpio_num.h"
#include "soc/clk_tree_defs.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "usb/uvc_host.h"

#if __has_include("cam/sensors/ov5647_sensor.h")
#include "cam/sensors/ov5647_sensor.h"
#endif

#if __has_include("esp_cam_ctlr_spi.h")
#include "esp_cam_ctlr_spi.h"
extern "C" __attribute__((weak)) esp_err_t esp_cam_new_spi_ctlr(
    const esp_cam_ctlr_spi_config_t *config, esp_cam_ctlr_handle_t *ret_handle) {
  (void)config;
  (void)ret_handle;
  return ESP_ERR_NOT_SUPPORTED;
}
#endif

static uint8_t cam_fps_from_name(const char *n) {
  if (!n) return 0;
  if (strstr(n, "60fps") || strstr(n, "60 fps")) return 60;
  if (strstr(n, "50fps") || strstr(n, "50 fps")) return 50;
  if (strstr(n, "30fps") || strstr(n, "30 fps")) return 30;
  if (strstr(n, "25fps") || strstr(n, "25 fps")) return 25;
  if (strstr(n, "15fps") || strstr(n, "15 fps")) return 15;
  if (strstr(n, "7fps")) return 7;
  return 0;
}

static uint8_t cam_fps_guess(esp32p4_cam_sensor_t s, uint16_t h) {
  switch (s) {
    case ESP32P4_SENSOR_OV9281:
    case ESP32P4_SENSOR_SC035HGS:
    case ESP32P4_SENSOR_STI2250:
      return 50;
    case ESP32P4_SENSOR_SC030IOT:
      return 60;
    case ESP32P4_SENSOR_MIRA220:
      return 15;
    case ESP32P4_SENSOR_OS02N10:
    case ESP32P4_SENSOR_OV2710:
    case ESP32P4_SENSOR_SC2331:
    case ESP32P4_SENSOR_GC2607:
      return 25;
    case ESP32P4_SENSOR_OV5640:
      return 14;
    case ESP32P4_SENSOR_OV2640:
      return 6;
    case ESP32P4_SENSOR_SP0A39:
      return 4;
    case ESP32P4_SENSOR_LT6911:
      return h >= 1000 ? 48 : 60;
    case ESP32P4_SENSOR_OV5647:
      return h >= 1000 ? 30 : 50;
    case ESP32P4_SENSOR_GC2145:
      return h >= 1200 ? 7 : (h >= 600 ? 30 : 15);
    default:
      return 30;
  }
}

static uint32_t cam_line_us(uint8_t fps, uint16_t h) {
  if (fps < 1) fps = 25;
  uint32_t vts = (uint32_t)h + 32u;
  if (vts < 48) vts = 48;
  uint32_t line = 1000000u / ((uint32_t)fps * vts);
  if (line < 8) line = 8;
  return line;
}

struct CamFbPool {
  SemaphoreHandle_t sem = nullptr;
  esp_cam_ctlr_trans_t trans[ESP32P4_CAM_FB_MAX]{};
  uint8_t *ptr[ESP32P4_CAM_FB_MAX]{};
  volatile uint8_t state[ESP32P4_CAM_FB_MAX]{};
  volatile int ready_q[ESP32P4_CAM_FB_MAX]{};
  volatile int ready_r = 0;
  volatile int ready_w = 0;
  volatile int ready_n = 0;
  volatile uint32_t new_trans = 0;
  volatile uint32_t done = 0;
  volatile uint32_t drop = 0;
  uint8_t n = 0;
};

struct ESP32P4_CamFbAccess {
  static CamFbPool *pool(ESP32P4_Camera *c) {
    return c ? static_cast<CamFbPool *>(c->_pool) : nullptr;
  }
  static camera_fb_t *fb(ESP32P4_Camera *c, int i) { return &c->_fb[i]; }
};

static ESP32P4_Camera *s_owner_csi = nullptr;
static ESP32P4_Camera *s_owner_dvp = nullptr;
static ESP32P4_Camera *s_owner_spi = nullptr;
static ESP32P4_Camera *s_owner_spi1 = nullptr;
static ESP32P4_Camera *s_owner_uvc = nullptr;
static int s_xclk_slot = 0;

static bool spi_second_host(const esp32p4_cam_config_t &cfg) {
  uint8_t p = cfg.spi.spi_port;
  return p != 0 && p != (uint8_t)SPI2_HOST;
}

struct CamSccbBind {
  explicit CamSccbBind(TwoWire *w) { esp32p4_sccb_lock(w); }
  ~CamSccbBind() { esp32p4_sccb_unlock(); }
};

static int cam_live_n() {
  int n = 0;
  if (s_owner_csi) n++;
  if (s_owner_dvp) n++;
  if (s_owner_spi) n++;
  if (s_owner_spi1) n++;
  if (s_owner_uvc) n++;
  return n;
}

bool esp32p4_cam_dual_ok(esp32p4_cam_bus_t a, esp32p4_cam_bus_t b) {
  return esp32p4_cam_dual_why(a, b) == nullptr;
}

const char *esp32p4_cam_dual_why(esp32p4_cam_bus_t a, esp32p4_cam_bus_t b) {
  if (a == ESP32P4_CAM_BUS_CSI && b == ESP32P4_CAM_BUS_CSI)
    return "ESP32-P4 has one MIPI CSI host";
  if (a == b && a == ESP32P4_CAM_BUS_SPI) return nullptr;
  if (a == b) return "same bus — P4 has one CSI / one DVP / one UVC-host (two SPI hosts OK)";
  return nullptr;
}

bool esp32p4_cam_dual_ok(const ESP32P4_Camera &a, const ESP32P4_Camera &b) {
  return esp32p4_cam_dual_why(a, b) == nullptr;
}

const char *esp32p4_cam_dual_why(const ESP32P4_Camera &a, const ESP32P4_Camera &b) {
  const char *w = esp32p4_cam_dual_why(a.bus(), b.bus());
  if (w) return w;
  if (a.bus() == ESP32P4_CAM_BUS_SPI && b.bus() == ESP32P4_CAM_BUS_SPI) {
    uint8_t pa = a.spiPort() ? a.spiPort() : (uint8_t)SPI2_HOST;
    uint8_t pb = b.spiPort() ? b.spiPort() : (uint8_t)SPI2_HOST;
    if (pa == pb) return "two SPI cameras need different spi_port (SPI2 vs SPI3 → /dev/video3 vs /dev/video4)";
  }
  return nullptr;
}
static bool s_usb_host_ok = false;
static TaskHandle_t s_usb_lib_task = nullptr;

enum : uint8_t { FB_FREE = 0, FB_CSI = 1, FB_READY = 2, FB_HELD = 3 };

static int IRAM_ATTR fb_index_from_buf(CamFbPool *p, const void *buf) {
  if (!p) return -1;
  for (uint8_t i = 0; i < p->n; i++) {
    if (p->ptr[i] == buf) return (int)i;
  }
  return -1;
}

static bool IRAM_ATTR ready_push(CamFbPool *p, int idx) {
  if (!p || p->ready_n >= (int)p->n) return false;
  p->ready_q[p->ready_w] = idx;
  p->ready_w = (p->ready_w + 1) % (p->n ? p->n : 1);
  p->ready_n++;
  return true;
}

static int IRAM_ATTR ready_pop(CamFbPool *p) {
  if (!p || p->ready_n <= 0) return -1;
  int idx = p->ready_q[p->ready_r];
  p->ready_r = (p->ready_r + 1) % (p->n ? p->n : 1);
  p->ready_n--;
  return idx;
}

static bool IRAM_ATTR on_get_new_trans(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *trans, void *ctx) {
  auto *p = ESP32P4_CamFbAccess::pool(static_cast<ESP32P4_Camera *>(ctx));
  if (!p || !trans) return false;
  p->new_trans++;
  for (uint8_t i = 0; i < p->n; i++) {
    if (p->state[i] == FB_FREE) {
      p->state[i] = FB_CSI;
      *trans = p->trans[i];
      return false;
    }
  }
  if (p->ready_n > 0) {
    int idx = ready_pop(p);
    if (idx >= 0 && idx < (int)p->n) {
      p->drop++;
      p->state[idx] = FB_CSI;
      *trans = p->trans[idx];
      return false;
    }
  }
  for (uint8_t i = 0; i < p->n; i++) {
    if (p->state[i] == FB_CSI) {
      p->drop++;
      *trans = p->trans[i];
      return false;
    }
  }
  *trans = p->trans[0];
  p->drop++;
  return false;
}

static bool IRAM_ATTR on_trans_finished(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *trans, void *ctx) {
  auto *p = ESP32P4_CamFbAccess::pool(static_cast<ESP32P4_Camera *>(ctx));
  if (!p) return false;
  p->done++;
  int idx = trans ? fb_index_from_buf(p, trans->buffer) : -1;
  if (idx < 0 || idx >= (int)p->n) {
    BaseType_t woken = pdFALSE;
    if (p->sem) xSemaphoreGiveFromISR(p->sem, &woken);
    return woken == pdTRUE;
  }
  if (p->ready_n >= (int)p->n - 1) {
    int old = ready_pop(p);
    if (old >= 0 && old < (int)p->n) {
      p->state[old] = FB_FREE;
      p->drop++;
    }
  }
  p->state[idx] = FB_READY;
  ready_push(p, idx);
  BaseType_t woken = pdFALSE;
  if (p->sem) xSemaphoreGiveFromISR(p->sem, &woken);
  return woken == pdTRUE;
}

static bool IRAM_ATTR on_uvc_frame(const uvc_host_frame_t *frame, void *ctx) {
  auto *cam = static_cast<ESP32P4_Camera *>(ctx);
  auto *p = ESP32P4_CamFbAccess::pool(cam);
  if (!p || !frame || !frame->data || !frame->data_len) return true;
  int idx = -1;
  for (uint8_t i = 0; i < p->n; i++) {
    if (p->state[i] == FB_FREE) {
      idx = (int)i;
      break;
    }
  }
  if (idx < 0 && p->ready_n > 0) {
    idx = ready_pop(p);
    p->drop++;
  }
  if (idx < 0 || idx >= (int)p->n) {
    p->drop++;
    return true;
  }
  size_t n = frame->data_len;
  if (n > p->trans[idx].buflen) n = p->trans[idx].buflen;
  memcpy(p->ptr[idx], frame->data, n);
  camera_fb_t *out = ESP32P4_CamFbAccess::fb(cam, idx);
  out->len = n;
  out->width = (uint16_t)frame->vs_format.h_res;
  out->height = (uint16_t)frame->vs_format.v_res;
  if (frame->vs_format.format == UVC_VS_FORMAT_YUY2) {
    out->format = ESP32P4_PIXFORMAT_YUYV;
  } else {
    out->format = ESP32P4_PIXFORMAT_JPEG;
  }
  p->done++;
  p->state[idx] = FB_READY;
  ready_push(p, idx);
  if (p->sem) xSemaphoreGive(p->sem);
  return true;
}

static void usb_lib_task(void *) {
  while (true) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      usb_host_device_free_all();
    }
  }
}

static bool start_xclk_gpio(int gpio, uint32_t hz) {
  if (s_xclk_slot > 3) {
    Serial.println("CSI: no free LEDC timer for XCLK (dual-cam)");
    return false;
  }
  const int slot = s_xclk_slot++;
  ledc_timer_config_t timer = {};
  timer.speed_mode = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = LEDC_TIMER_1_BIT;
  timer.timer_num = (ledc_timer_t)slot;
  timer.freq_hz = hz;
  timer.clk_cfg = LEDC_USE_XTAL_CLK;
  if (ledc_timer_config(&timer) != ESP_OK) {
    timer.clk_cfg = LEDC_AUTO_CLK;
    timer.freq_hz = 20000000;
    if (ledc_timer_config(&timer) != ESP_OK) return false;
    Serial.printf("CSI: XCLK fell back to 20 MHz on GPIO%d\n", gpio);
  }
  ledc_channel_config_t ch = {};
  ch.gpio_num = gpio;
  ch.speed_mode = LEDC_LOW_SPEED_MODE;
  ch.channel = (ledc_channel_t)slot;
  ch.timer_sel = (ledc_timer_t)slot;
  ch.duty = 1;
  ch.hpoint = 0;
  if (ledc_channel_config(&ch) != ESP_OK) return false;
  Serial.printf("CSI: XCLK %u Hz on GPIO%d (LEDC %d)\n", (unsigned)timer.freq_hz, gpio, slot);
  return true;
}

esp32p4_cam_config_t esp32p4_cam_config_default() {
  return esp32p4_cam_config_board(ESP32P4_BOARD_GUITION_M3);
}

esp32p4_cam_config_t esp32p4_cam_config_board(esp32p4_board_t board) {
  esp32p4_cam_config_t c{};
  c.sda = 7;
  c.scl = 8;
  c.wire = &Wire;
  c.xclk = -1;
  c.pwdn = -1;
  c.reset = -1;
  c.xclk_hz = 24000000;
  c.i2c_addr = 0;
  c.ldo_chan = 3;
  c.ldo_mv = 2500;
  c.frame_size = ESP32P4_FRAMESIZE_AUTO;
  c.pixel_format = ESP32P4_PIXFORMAT_RGB565;
  c.lane_bit_rate_mbps = 0;  // auto from mode
  c.sensor = ESP32P4_SENSOR_AUTO;
  c.test_pattern = false;
  c.fb_count = 3;  // 1 held by app + 1 CSI + 1 spare (prevents mid-frame tear)
  c.csi_id = 0;
  c.bus = ESP32P4_CAM_BUS_CSI;
  for (int i = 0; i < 16; i++) c.dvp.data[i] = -1;
  c.dvp.vsync = -1;
  c.dvp.de = -1;
  c.dvp.pclk = -1;
  c.dvp.xclk = -1;
  c.dvp.data_width = 8;
  c.spi.cs = -1;
  c.spi.sclk = -1;
  c.spi.d0 = -1;
  c.spi.d1 = -1;
  c.spi.d2 = -1;
  c.spi.d3 = -1;
  c.spi.xclk = -1;
  c.spi.spi_port = 0;
  c.spi.io_mode = 0;
  c.spi.intf = 0;
  c.uvc.vid = 0;
  c.uvc.pid = 0;
  c.uvc.dev_addr = 0;
  c.uvc.format = 1;  // MJPEG
  c.uvc.width = 640;
  c.uvc.height = 480;
  c.uvc.fps = 0;

  switch (board) {
    case ESP32P4_BOARD_FUNCTION_EV:
      // Espressif ESP32-P4-Function-EV-Board CSI (SC2336 typical)
      c.sda = 7;
      c.scl = 8;
      c.xclk = -1;  // often module crystal; set GPIO if your carrier needs SoC MCLK
      c.pwdn = -1;
      c.reset = -1;
      break;
    case ESP32P4_BOARD_WAVESHARE_NANO:
      // Waveshare ESP32-P4-Nano CSI (OV5647 module common)
      c.sda = 7;
      c.scl = 8;
      c.xclk = -1;
      break;
    case ESP32P4_BOARD_GUITION_M3:
    case ESP32P4_BOARD_CUSTOM:
    default:
      break;
  }
  return c;
}

const char *ESP32P4_Camera::sensorName() const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (ops && ops->name) return ops->name;
  return "unknown";
}

uint32_t ESP32P4_Camera::newTransCount() const {
  auto *p = ESP32P4_CamFbAccess::pool(const_cast<ESP32P4_Camera *>(this));
  return p ? p->new_trans : 0;
}
uint32_t ESP32P4_Camera::doneCount() const {
  auto *p = ESP32P4_CamFbAccess::pool(const_cast<ESP32P4_Camera *>(this));
  return p ? p->done : 0;
}
uint32_t ESP32P4_Camera::dropCount() const {
  auto *p = ESP32P4_CamFbAccess::pool(const_cast<ESP32P4_Camera *>(this));
  return p ? p->drop : 0;
}

uint8_t *ESP32P4_Camera::fbBuf(uint8_t index) const {
  auto *p = ESP32P4_CamFbAccess::pool(const_cast<ESP32P4_Camera *>(this));
  if (!p || index >= p->n) return nullptr;
  return p->ptr[index];
}
bool ESP32P4_Camera::psramOk() const { return esp32p4_psram_available(); }

const char *ESP32P4_Camera::busName() const {
  switch (_bus) {
    case ESP32P4_CAM_BUS_DVP: return "DVP";
    case ESP32P4_CAM_BUS_SPI: return "SPI";
    case ESP32P4_CAM_BUS_UVC_HOST: return "UVC-HOST";
    case ESP32P4_CAM_BUS_CSI:
    default: return "CSI";
  }
}

const char *esp32p4_pixformat_name(esp32p4_cam_pixformat_t fmt) {
  switch (fmt) {
    case ESP32P4_PIXFORMAT_RGB888: return "RGB888";
    case ESP32P4_PIXFORMAT_YUV422: return "YUV422";
    case ESP32P4_PIXFORMAT_YUV420: return "YUV420";
    case ESP32P4_PIXFORMAT_GRAY8: return "GRAY8";
    case ESP32P4_PIXFORMAT_RAW8: return "RAW8";
    case ESP32P4_PIXFORMAT_RAW10: return "RAW10";
    case ESP32P4_PIXFORMAT_RAW12: return "RAW12";
    case ESP32P4_PIXFORMAT_YUYV: return "YUYV";
    case ESP32P4_PIXFORMAT_JPEG: return "JPEG";
    case ESP32P4_PIXFORMAT_RGB565:
    default: return "RGB565";
  }
}

esp32p4_cam_pixformat_t esp32p4_pixformat_pipe(esp32p4_cam_pixformat_t fmt) {
  return (fmt == ESP32P4_PIXFORMAT_JPEG) ? ESP32P4_PIXFORMAT_RGB565 : fmt;
}

uint32_t esp32p4_pixformat_fourcc(esp32p4_cam_pixformat_t fmt) {
  auto fcc = [](char a, char b, char c, char d) -> uint32_t {
    return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
  };
  switch (fmt) {
    case ESP32P4_PIXFORMAT_RGB888: return fcc('R', 'G', 'B', '3');
    case ESP32P4_PIXFORMAT_YUV422: return fcc('U', 'Y', 'V', 'Y');
    case ESP32P4_PIXFORMAT_YUYV: return fcc('Y', 'U', 'Y', 'V');
    case ESP32P4_PIXFORMAT_YUV420: return fcc('Y', 'U', '1', '2');
    case ESP32P4_PIXFORMAT_GRAY8: return fcc('G', 'R', 'E', 'Y');
    case ESP32P4_PIXFORMAT_RAW8: return fcc('B', 'A', '8', '1');
    case ESP32P4_PIXFORMAT_RAW10: return fcc('B', 'G', '1', '0');
    case ESP32P4_PIXFORMAT_RAW12: return fcc('B', 'G', '1', '2');
    case ESP32P4_PIXFORMAT_JPEG: return fcc('J', 'P', 'E', 'G');
    case ESP32P4_PIXFORMAT_RGB565:
    default: return fcc('R', 'G', 'B', 'P');
  }
}

size_t esp32p4_pixformat_fb_bytes(esp32p4_cam_pixformat_t fmt, uint16_t w, uint16_t h) {
  const size_t px = (size_t)w * (size_t)h;
  switch (esp32p4_pixformat_pipe(fmt)) {
    case ESP32P4_PIXFORMAT_RGB888: return px * 3;
    case ESP32P4_PIXFORMAT_YUV420:
    case ESP32P4_PIXFORMAT_GRAY8: return px + (px / 2);
    case ESP32P4_PIXFORMAT_RAW8: return px;
    case ESP32P4_PIXFORMAT_RAW10:
    case ESP32P4_PIXFORMAT_RAW12:
    case ESP32P4_PIXFORMAT_YUYV:
    case ESP32P4_PIXFORMAT_YUV422:
    case ESP32P4_PIXFORMAT_RGB565:
    default: return px * 2;
  }
}

bool ESP32P4_Camera::probe_sensor() {
  uint8_t a = 0;
  const esp32p4_cam_sensor_ops_t *ops = esp32p4_cam_sensor_probe_bus(_cfg.sensor, &a, _bus);
  if (!ops) return false;
  if (ops->support == ESP32P4_CAM_SUPPORT_DETECT_ONLY) {
    Serial.printf("CSI: %s detected but no mode table yet (DETECT_ONLY)\n", ops->name);
    return false;
  }
  _ops = ops;
  _addr = a;
  _sensor = ops->id;
  return true;
}

bool ESP32P4_Camera::init_sensor() {
  CamSccbBind i2c_bind(_cfg.wire);
  _sensor = ESP32P4_SENSOR_AUTO;
  _ops = nullptr;
  if (_cfg.pwdn >= 0) {
    pinMode(_cfg.pwdn, OUTPUT);
    digitalWrite(_cfg.pwdn, LOW);
    delay(10);
  }
  if (_cfg.reset >= 0) {
    pinMode(_cfg.reset, OUTPUT);
    digitalWrite(_cfg.reset, LOW);
    delay(5);
    digitalWrite(_cfg.reset, HIGH);
    delay(20);
  }
  // DVP controller can drive XCLK; skip LEDC so we do not double-clock the pin.
  const bool dvp_owns_xclk = (_bus == ESP32P4_CAM_BUS_DVP && _cfg.dvp.xclk >= 0);
  if (_cfg.xclk >= 0 && !dvp_owns_xclk) {
    if (!start_xclk_gpio(_cfg.xclk, _cfg.xclk_hz ? _cfg.xclk_hz : 24000000)) return false;
    delay(20);
  }

  TwoWire &i2c = _cfg.wire ? *_cfg.wire : Wire;
  i2c.begin(_cfg.sda, _cfg.scl);
  i2c.setClock(100000);
  delay(20);

  const char *bus_name = (&i2c == &Wire) ? "Wire" : "Wire1";
  Serial.printf("CSI: I2C scan SDA=%d SCL=%d  bus=%s  registry=%u sensors\n", _cfg.sda, _cfg.scl,
                bus_name, (unsigned)esp32p4_cam_sensor_registry_count());
  int found = 0;
  for (int a = 1; a < 127; a++) {
    i2c.beginTransmission(a);
    if (i2c.endTransmission() == 0) {
      Serial.printf("  ack 0x%02X\n", a);
      found++;
    }
  }
  if (!found) Serial.println("  (no devices - check CSI ribbon)");

  if (!probe_sensor()) {
    Serial.printf("CSI: no supported %s sensor with a usable mode\n", busName());
    return false;
  }

  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  esp32p4_cam_mode_t mode{};
  if (!ops->configure || !ops->configure((uint8_t)_addr, _cfg.frame_size, &mode)) {
    Serial.printf("CSI: %s configure failed\n", ops->name);
    return false;
  }
  _w = mode.width;
  _h = mode.height;
  {
    uint8_t fps = mode.fps;
    if (!fps) fps = cam_fps_from_name(mode.name);
    if (!fps) fps = cam_fps_guess(_sensor, _h);
    _fps = fps;
    _line_us = cam_line_us(_fps, _h);
  }
  _lanes = mode.lanes ? mode.lanes : 2;
  _bayer = (uint8_t)mode.bayer;
  _raw8 = (mode.in_fmt == ESP32P4_CAM_IN_RAW8);
  _in_fmt = (uint8_t)mode.in_fmt;
  _use_isp = (mode.in_fmt == ESP32P4_CAM_IN_RAW8 || mode.in_fmt == ESP32P4_CAM_IN_RAW10);
  if (_cfg.lane_bit_rate_mbps <= 0) _cfg.lane_bit_rate_mbps = mode.lane_mbps > 0 ? mode.lane_mbps : 400;
  const esp32p4_cam_pixformat_t want = _cfg.pixel_format;
  if (_use_isp && (_w > 1920 || _h > 1080)) {
    if (want != ESP32P4_PIXFORMAT_RAW8 && want != ESP32P4_PIXFORMAT_RAW10 &&
        want != ESP32P4_PIXFORMAT_RAW12) {
      Serial.printf("CSI: %ux%u exceeds ISP 1920x1080 - RAW10 bypass (no demosaic)\n", _w, _h);
      _cfg.pixel_format = ESP32P4_PIXFORMAT_RAW10;
    }
  }
  if (want == ESP32P4_PIXFORMAT_RAW12) {
    Serial.println("CSI: RAW12 requested - no RAW12 sensor table; using RAW10");
    _cfg.pixel_format = ESP32P4_PIXFORMAT_RAW10;
  }
  _cfg.pixel_format = resolve_format(_cfg.pixel_format);
  Serial.printf("CSI: %s mode %s  %ux%u @ %ufps  line=%uus  lanes=%u  %d Mbps/lane  isp=%d  fmt=%s\n",
                ops->name, mode.name ? mode.name : "?", _w, _h, (unsigned)_fps, (unsigned)_line_us,
                (unsigned)_lanes, _cfg.lane_bit_rate_mbps, _use_isp ? 1 : 0,
                esp32p4_pixformat_name(_cfg.pixel_format));
  return true;
}

esp32p4_cam_pixformat_t ESP32P4_Camera::resolve_format(esp32p4_cam_pixformat_t want) const {
  if (want == ESP32P4_PIXFORMAT_JPEG) return want;
  if (want == ESP32P4_PIXFORMAT_RAW12) return ESP32P4_PIXFORMAT_RAW10;
  if (_use_isp) return want;
  if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_YUV422) {
    if (want == ESP32P4_PIXFORMAT_YUYV) return ESP32P4_PIXFORMAT_YUYV;
    return ESP32P4_PIXFORMAT_YUV422;
  }
  if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_GRAY8) return ESP32P4_PIXFORMAT_GRAY8;
  if (want == ESP32P4_PIXFORMAT_RGB565 || want == ESP32P4_PIXFORMAT_YUV422 ||
      want == ESP32P4_PIXFORMAT_YUYV)
    return want;
  return ESP32P4_PIXFORMAT_RGB565;
}

bool ESP32P4_Camera::supportsFormat(esp32p4_cam_pixformat_t fmt) const {
  if (fmt == ESP32P4_PIXFORMAT_JPEG) return true;
  if (fmt == ESP32P4_PIXFORMAT_RAW12) return false;
  fmt = resolve_format(fmt);
  if (_use_isp) return true;
  if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_YUV422)
    return fmt == ESP32P4_PIXFORMAT_YUV422 || fmt == ESP32P4_PIXFORMAT_YUYV;
  if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_GRAY8) return fmt == ESP32P4_PIXFORMAT_GRAY8;
  return fmt == ESP32P4_PIXFORMAT_RGB565 || fmt == ESP32P4_PIXFORMAT_YUV422 ||
         fmt == ESP32P4_PIXFORMAT_YUYV;
}

bool ESP32P4_Camera::init_mipi_ldo() {
  esp_ldo_channel_config_t ldo_cfg = {};
  ldo_cfg.chan_id = _cfg.ldo_chan > 0 ? _cfg.ldo_chan : 3;
  ldo_cfg.voltage_mv = _cfg.ldo_mv > 0 ? _cfg.ldo_mv : 2500;
  esp_ldo_channel_handle_t ldo = nullptr;
  if (esp_ldo_acquire_channel(&ldo_cfg, &ldo) != ESP_OK) {
    Serial.printf("CSI: LDO acquire failed chan=%d %dmV\n", ldo_cfg.chan_id, ldo_cfg.voltage_mv);
    return false;
  }
  _ldo = ldo;
  return true;
}

bool ESP32P4_Camera::alloc_fbs() {
  _fb_n = _cfg.fb_count;
  if (_fb_n < 2) _fb_n = 2;
  if (_fb_n > ESP32P4_CAM_FB_MAX) _fb_n = ESP32P4_CAM_FB_MAX;
  if (!_pool) _pool = new CamFbPool();
  auto *p = ESP32P4_CamFbAccess::pool(this);
  if (!p) return false;
  if (!p->sem) {
    p->sem = xSemaphoreCreateBinary();
    if (!p->sem) return false;
  }
  while (xSemaphoreTake(p->sem, 0) == pdTRUE) {
  }
  _fb_cap = esp32p4_pixformat_fb_bytes(_cfg.pixel_format, _w, _h);
  if (_cfg.pixel_format == ESP32P4_PIXFORMAT_GRAY8 && !_use_isp) {
    _fb_cap = (size_t)_w * (size_t)_h;
  }
  if (_bus == ESP32P4_CAM_BUS_UVC_HOST && _cfg.pixel_format == ESP32P4_PIXFORMAT_JPEG) {
    if (_fb_cap < 256 * 1024) _fb_cap = 256 * 1024;
  }
  if (_fb_cap < (size_t)_w * 2) _fb_cap = (size_t)_w * (size_t)_h * 2;
  for (uint8_t i = 0; i < _fb_n; i++) {
    uint8_t *buf = (uint8_t *)esp32p4_psram_alloc(_fb_cap);
    if (!buf) {
      Serial.printf("CSI: FB%u alloc failed (%u bytes) PSRAM free=%u\n", i, (unsigned)_fb_cap,
                    (unsigned)esp32p4_psram_free_size());
      return false;
    }
    _fb[i].buf = buf;
    _fb[i].len = _fb_cap;
    _fb[i].width = _w;
    _fb[i].height = _h;
    _fb[i].format = _cfg.pixel_format;
    _fb[i].timestamp_us = 0;
    p->trans[i].buffer = buf;
    p->trans[i].buflen = _fb_cap;
    p->ptr[i] = buf;
    p->state[i] = FB_FREE;
  }
  for (uint8_t i = _fb_n; i < ESP32P4_CAM_FB_MAX; i++) {
    p->ptr[i] = nullptr;
    p->state[i] = FB_FREE;
    p->trans[i] = {};
  }
  p->n = _fb_n;
  p->ready_r = 0;
  p->ready_w = 0;
  p->ready_n = 0;
  p->drop = 0;
  p->new_trans = 0;
  p->done = 0;
  Serial.printf("CSI: %u PSRAM framebuffers x %u bytes (%s, PSRAM free=%u)\n", _fb_n,
                (unsigned)_fb_cap, esp32p4_pixformat_name(_cfg.pixel_format),
                (unsigned)esp32p4_psram_free_size());
  return true;
}

void ESP32P4_Camera::free_fbs() {
  auto *p = ESP32P4_CamFbAccess::pool(this);
  for (uint8_t i = 0; i < _fb_n; i++) {
    esp32p4_psram_free(_fb[i].buf);
    _fb[i] = {};
    if (p) {
      p->trans[i] = {};
      p->ptr[i] = nullptr;
      p->state[i] = FB_FREE;
    }
  }
  _fb_n = 0;
  if (p) {
    p->n = 0;
    p->ready_n = 0;
  }
}

static color_raw_element_order_t bayer_to_idf(uint8_t b) {
  switch ((esp32p4_cam_bayer_t)b) {
    case ESP32P4_BAYER_GRBG: return COLOR_RAW_ELEMENT_ORDER_GRBG;
    case ESP32P4_BAYER_GBRG: return COLOR_RAW_ELEMENT_ORDER_GBRG;
    case ESP32P4_BAYER_BGGR: return COLOR_RAW_ELEMENT_ORDER_BGGR;
    case ESP32P4_BAYER_RGGB:
    default: return COLOR_RAW_ELEMENT_ORDER_RGGB;
  }
}

bool ESP32P4_Camera::init_csi_isp() {
  const int bitrate = _cfg.lane_bit_rate_mbps > 0 ? _cfg.lane_bit_rate_mbps : 200;
  const uint8_t lanes = _lanes ? _lanes : 2;
  const bool sensor_yuv = (!_use_isp && _in_fmt == (uint8_t)ESP32P4_CAM_IN_YUV422);
  const bool sensor_rgb565 = (!_use_isp && !sensor_yuv);
  const bool raw_out = (_cfg.pixel_format == ESP32P4_PIXFORMAT_RAW8 ||
                        _cfg.pixel_format == ESP32P4_PIXFORMAT_RAW10 ||
                        _cfg.pixel_format == ESP32P4_PIXFORMAT_RAW12);
  const bool use_isp = _use_isp && !raw_out;

  isp_color_t isp_out = ISP_COLOR_RGB565;
  cam_ctlr_color_t csi_out = CAM_CTLR_COLOR_RGB565;
  switch (esp32p4_pixformat_pipe(_cfg.pixel_format)) {
    case ESP32P4_PIXFORMAT_RGB888:
      isp_out = ISP_COLOR_RGB888;
      csi_out = CAM_CTLR_COLOR_RGB888;
      break;
    case ESP32P4_PIXFORMAT_YUYV:
    case ESP32P4_PIXFORMAT_YUV422:
      isp_out = ISP_COLOR_YUV422;
      csi_out = CAM_CTLR_COLOR_YUV422;
      break;
    case ESP32P4_PIXFORMAT_YUV420:
    case ESP32P4_PIXFORMAT_GRAY8:
      isp_out = ISP_COLOR_YUV420;
      csi_out = CAM_CTLR_COLOR_YUV420;
      break;
    case ESP32P4_PIXFORMAT_RAW8:
      csi_out = CAM_CTLR_COLOR_RAW8;
      break;
    case ESP32P4_PIXFORMAT_RAW10:
      csi_out = CAM_CTLR_COLOR_RAW10;
      break;
    case ESP32P4_PIXFORMAT_RAW12:
      csi_out = CAM_CTLR_COLOR_RAW12;
      break;
    case ESP32P4_PIXFORMAT_RGB565:
    default:
      break;
  }

  if (use_isp) {
    esp_isp_processor_cfg_t isp_cfg = {};
    isp_cfg.clk_hz = (_w >= 1600) ? (160 * 1000 * 1000) : (80 * 1000 * 1000);
    isp_cfg.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
    isp_cfg.input_data_color_type = _raw8 ? ISP_COLOR_RAW8 : ISP_COLOR_RAW10;
    isp_cfg.output_data_color_type = isp_out;
    isp_cfg.yuv_range = ISP_COLOR_RANGE_FULL;
    isp_cfg.yuv_std = ISP_YUV_CONV_STD_BT601;
    isp_cfg.has_line_start_packet = false;
    isp_cfg.has_line_end_packet = false;
    isp_cfg.h_res = _w;
    isp_cfg.v_res = _h;
    isp_cfg.bayer_order = bayer_to_idf(_bayer);
    isp_proc_handle_t isp = nullptr;
    if (esp_isp_new_processor(&isp_cfg, &isp) != ESP_OK) return false;
    if (esp_isp_enable(isp) != ESP_OK) {
      esp_isp_del_processor(isp);
      return false;
    }
    esp_isp_demosaic_config_t demosaic = {};
    demosaic.padding_mode = ISP_DEMOSAIC_EDGE_PADDING_MODE_SRND_DATA;
    if (esp_isp_demosaic_configure(isp, &demosaic) != ESP_OK ||
        esp_isp_demosaic_enable(isp) != ESP_OK) {
      esp_isp_del_processor(isp);
      return false;
    }
    _isp = isp;
    if (!_isp_pipe) _isp_pipe = new ESP32P4_Isp();
    if (_isp_pipe && _isp_pipe->begin(isp, _w, _h, _sensor)) {
#if __has_include("cam/sensors/ov5647_sensor.h")
      if (_sensor == ESP32P4_SENSOR_OV5647) {
        uint16_t mx = ov5647_exposure_max_lines();
        if (mx >= 32) _isp_pipe->setAeLimits(8, mx, 16, 160);
      }
#endif
      Serial.printf("CSI: ISP RAW->%s + IPA %s%s%s (clk=%u MHz bayer=%u)\n",
                    esp32p4_pixformat_name(esp32p4_pixformat_pipe(_cfg.pixel_format)), _isp_pipe->profileName(),
                    _isp_pipe->lscEnabled() ? " LSC" : "",
                    _isp_pipe->blcEnabled() ? " BLC" : "",
                    (unsigned)(isp_cfg.clk_hz / 1000000), (unsigned)_bayer);
    } else {
      Serial.printf("CSI: ISP RAW->%s demosaic only (3A unavailable)\n",
                    esp32p4_pixformat_name(esp32p4_pixformat_pipe(_cfg.pixel_format)));
    }
  }

  esp_cam_ctlr_csi_config_t csi_cfg = {};
  csi_cfg.ctlr_id = _cfg.csi_id;
  csi_cfg.h_res = _w;
  csi_cfg.v_res = _h;
  csi_cfg.data_lane_num = lanes;
  csi_cfg.lane_bit_rate_mbps = bitrate;
  if (sensor_yuv) {
    csi_cfg.input_data_color_type = CAM_CTLR_COLOR_YUV422;
    csi_cfg.output_data_color_type = CAM_CTLR_COLOR_YUV422;
  } else if (sensor_rgb565) {
    csi_cfg.input_data_color_type = CAM_CTLR_COLOR_RGB565;
    csi_cfg.output_data_color_type = CAM_CTLR_COLOR_RGB565;
  } else if (raw_out) {
    csi_cfg.input_data_color_type = _raw8 ? CAM_CTLR_COLOR_RAW8 : CAM_CTLR_COLOR_RAW10;
    csi_cfg.output_data_color_type = csi_out;
  } else {
    csi_cfg.input_data_color_type = _raw8 ? CAM_CTLR_COLOR_RAW8 : CAM_CTLR_COLOR_RAW10;
    csi_cfg.output_data_color_type = csi_out;
  }
  csi_cfg.queue_items = (_w >= 1600) ? 2 : 1;
  csi_cfg.byte_swap_en = false;

  esp_cam_ctlr_handle_t cam = nullptr;
  if (esp_cam_new_csi_ctlr(&csi_cfg, &cam) != ESP_OK) return false;
  _cam = cam;

  esp_cam_ctlr_evt_cbs_t cbs = {};
  cbs.on_get_new_trans = on_get_new_trans;
  cbs.on_trans_finished = on_trans_finished;
  if (esp_cam_ctlr_register_event_callbacks(cam, &cbs, this) != ESP_OK) return false;
  if (esp_cam_ctlr_enable(cam) != ESP_OK) return false;
  if (esp_cam_ctlr_start(cam) != ESP_OK) return false;
  _started = true;
  Serial.printf("CSI: controller started %ux%u  %u-lane @ %d Mbps/lane  %s\n", _w, _h,
                (unsigned)lanes, bitrate, esp32p4_pixformat_name(_cfg.pixel_format));
  return true;
}

bool ESP32P4_Camera::start_sensor_stream() {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->stream_on) return false;
  if (!ops->stream_on((uint8_t)_addr)) return false;
  if (_cfg.test_pattern && ops->set_test_pattern) {
    ops->set_test_pattern((uint8_t)_addr, true);
  }
  delay(200);
  return true;
}

bool ESP32P4_Camera::claim_bus() {
  ESP32P4_Camera **slot = nullptr;
  const char *name = busName();
  switch (_bus) {
    case ESP32P4_CAM_BUS_DVP: slot = &s_owner_dvp; break;
    case ESP32P4_CAM_BUS_SPI:
      slot = spi_second_host(_cfg) ? &s_owner_spi1 : &s_owner_spi;
      break;
    case ESP32P4_CAM_BUS_UVC_HOST: slot = &s_owner_uvc; break;
    case ESP32P4_CAM_BUS_CSI:
    default: slot = &s_owner_csi; break;
  }
  if (*slot && *slot != this) {
    Serial.printf("CSI: %s bus already in use by another ESP32P4_Camera\n", name);
    return false;
  }
  *slot = this;
  return true;
}

void ESP32P4_Camera::release_bus() {
  if (s_owner_csi == this) s_owner_csi = nullptr;
  if (s_owner_dvp == this) s_owner_dvp = nullptr;
  if (s_owner_spi == this) s_owner_spi = nullptr;
  if (s_owner_spi1 == this) s_owner_spi1 = nullptr;
  if (s_owner_uvc == this) s_owner_uvc = nullptr;
}

static void note_dual_live() {
  if (cam_live_n() < 2) return;
  Serial.printf("CSI: dual capture live");
  if (s_owner_csi) Serial.printf(" CSI");
  if (s_owner_dvp) Serial.printf(" DVP");
  if (s_owner_spi) Serial.printf(" SPI");
  if (s_owner_spi1) Serial.printf(" SPI1");
  if (s_owner_uvc) Serial.printf(" UVC-HOST");
  Serial.println("  (not two CSI)");
}

bool ESP32P4_Camera::init_dvp() {
  if (_cfg.dvp.pclk < 0 || _cfg.dvp.vsync < 0) {
    Serial.println("CSI: DVP needs pclk + vsync GPIOs (cfg.dvp)");
    return false;
  }
  const uint8_t width = _cfg.dvp.data_width ? _cfg.dvp.data_width : 8;
  esp_cam_ctlr_dvp_pin_config_t pin = {};
  pin.data_width = (width >= 16) ? CAM_CTLR_DATA_WIDTH_16 : CAM_CTLR_DATA_WIDTH_8;
  for (int i = 0; i < CAM_DVP_DATA_SIG_NUM; i++) {
    pin.data_io[i] = (_cfg.dvp.data[i] >= 0) ? (gpio_num_t)_cfg.dvp.data[i] : GPIO_NUM_NC;
  }
  pin.vsync_io = (gpio_num_t)_cfg.dvp.vsync;
  pin.de_io = (_cfg.dvp.de >= 0) ? (gpio_num_t)_cfg.dvp.de : GPIO_NUM_NC;
  pin.pclk_io = (gpio_num_t)_cfg.dvp.pclk;
  pin.xclk_io = (_cfg.dvp.xclk >= 0) ? (gpio_num_t)_cfg.dvp.xclk : GPIO_NUM_NC;

  cam_ctlr_color_t color = CAM_CTLR_COLOR_RGB565;
  if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_YUV422) color = CAM_CTLR_COLOR_YUV422;
  else if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_GRAY8) color = CAM_CTLR_COLOR_GRAY8;

  esp_cam_ctlr_dvp_config_t dvp = {};
  dvp.ctlr_id = 0;
  dvp.clk_src = CAM_CLK_SRC_DEFAULT;
  dvp.h_res = _w;
  dvp.v_res = _h;
  dvp.input_data_color_type = color;
  dvp.cam_data_width = width;
  dvp.bk_buffer_dis = 1;
  dvp.external_xtal = (_cfg.dvp.xclk < 0) ? 1 : 0;
  dvp.xclk_freq = _cfg.xclk_hz ? _cfg.xclk_hz : 20000000;
  dvp.pin = &pin;

  esp_cam_ctlr_handle_t cam = nullptr;
  if (esp_cam_new_dvp_ctlr(&dvp, &cam) != ESP_OK) {
    Serial.println("CSI: DVP controller create failed");
    return false;
  }
  _cam = cam;
  esp_cam_ctlr_evt_cbs_t cbs = {};
  cbs.on_get_new_trans = on_get_new_trans;
  cbs.on_trans_finished = on_trans_finished;
  if (esp_cam_ctlr_register_event_callbacks(cam, &cbs, this) != ESP_OK) return false;
  if (esp_cam_ctlr_enable(cam) != ESP_OK) return false;
  if (esp_cam_ctlr_start(cam) != ESP_OK) return false;
  _started = true;
  Serial.printf("CSI: DVP started %ux%u  %u-bit  %s  xclk=%u Hz\n", _w, _h, (unsigned)width,
                esp32p4_pixformat_name(_cfg.pixel_format), (unsigned)dvp.xclk_freq);
  return true;
}

bool ESP32P4_Camera::init_spi() {
#if !__has_include("esp_cam_ctlr_spi.h")
  Serial.println("CSI: SPI camera not supported in this core version");
  return false;
#else
  CamSccbBind i2c_bind(_cfg.wire);
  if (_cfg.spi.cs < 0 || _cfg.spi.sclk < 0 || _cfg.spi.d0 < 0) {
    Serial.println("CSI: SPI cam needs cs + sclk + d0 GPIOs (cfg.spi)");
    return false;
  }
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  (void)ops;
  esp32p4_cam_mode_t mode{};
  // Re-read frame_info from last configure: stored on a dummy configure is not kept.
  // Probe again into a local mode via configure is already done in init_sensor.
  // We kept spi_frame_info only on the stack then. Reconfigure to recover it.
  if (ops && ops->configure) {
    if (!ops->configure((uint8_t)_addr, _cfg.frame_size, &mode)) return false;
  }
  auto *info = (const esp_cam_sensor_spi_frame_info *)mode.spi_frame_info;
  if (!info) {
    Serial.println("CSI: SPI sensor has no frame_info");
    return false;
  }

  cam_ctlr_color_t color = CAM_CTLR_COLOR_GRAY8;
  if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_YUV422) color = CAM_CTLR_COLOR_YUV422;
  else if (_in_fmt == (uint8_t)ESP32P4_CAM_IN_RGB565) color = CAM_CTLR_COLOR_RGB565;

  esp_cam_ctlr_spi_config_t spi = {};
  spi.intf = (_cfg.spi.intf == 1) ? ESP_CAM_CTLR_SPI_CAM_INTF_PARLIO : ESP_CAM_CTLR_SPI_CAM_INTF_SPI;
  spi.io_mode = (_cfg.spi.io_mode == 2)   ? ESP_CAM_CTLR_SPI_CAM_IO_MODE_4BIT
                : (_cfg.spi.io_mode == 1) ? ESP_CAM_CTLR_SPI_CAM_IO_MODE_2BIT
                                          : ESP_CAM_CTLR_SPI_CAM_IO_MODE_1BIT;
  spi.spi_port = (_cfg.spi.spi_port > 0) ? (spi_host_device_t)_cfg.spi.spi_port : SPI2_HOST;
  spi.spi_cs_pin = (gpio_num_t)_cfg.spi.cs;
  spi.spi_sclk_pin = (gpio_num_t)_cfg.spi.sclk;
  spi.spi_data0_io_pin = (gpio_num_t)_cfg.spi.d0;
  spi.spi_data1_io_pin = (_cfg.spi.d1 >= 0) ? (gpio_num_t)_cfg.spi.d1 : GPIO_NUM_NC;
  spi.spi_data2_io_pin = (_cfg.spi.d2 >= 0) ? (gpio_num_t)_cfg.spi.d2 : GPIO_NUM_NC;
  spi.spi_data3_io_pin = (_cfg.spi.d3 >= 0) ? (gpio_num_t)_cfg.spi.d3 : GPIO_NUM_NC;
  spi.reset_pin = (_cfg.reset >= 0) ? (gpio_num_t)_cfg.reset : GPIO_NUM_NC;
  spi.pwdn_pin = (_cfg.pwdn >= 0) ? (gpio_num_t)_cfg.pwdn : GPIO_NUM_NC;
  spi.h_res = _w;
  spi.v_res = _h;
  spi.input_data_color_type = color;
  spi.frame_info = info;
  spi.frame_buffer_count = _fb_n;
  spi.bk_buffer_dis = 1;

  if (_cfg.spi.xclk >= 0 && _cfg.xclk < 0) {
    if (!start_xclk_gpio(_cfg.spi.xclk, _cfg.xclk_hz ? _cfg.xclk_hz : 24000000)) return false;
  }

  esp_cam_ctlr_handle_t cam = nullptr;
  esp_err_t err = esp_cam_new_spi_ctlr(&spi, &cam);
  if (err != ESP_OK) {
    Serial.printf("CSI: SPI controller create failed (%d) - Arduino P4 lib may omit SPI cam\n",
                  (int)err);
    return false;
  }
  _cam = cam;
  esp_cam_ctlr_evt_cbs_t cbs = {};
  cbs.on_get_new_trans = on_get_new_trans;
  cbs.on_trans_finished = on_trans_finished;
  if (esp_cam_ctlr_register_event_callbacks(cam, &cbs, this) != ESP_OK) return false;
  if (esp_cam_ctlr_enable(cam) != ESP_OK) return false;
  if (esp_cam_ctlr_start(cam) != ESP_OK) return false;
  _started = true;
  Serial.printf("CSI: SPI started %ux%u  %s\n", _w, _h, esp32p4_pixformat_name(_cfg.pixel_format));
  return true;
#endif
}

bool ESP32P4_Camera::init_uvc_host() {
  Serial.println("CSI: USB host UVC - do not run ESP32P4_Uvc gadget in the same sketch");
  Serial.println("CSI: Prefer USB Mode that leaves OTG as host (not TinyUSB device)");
  if (!s_usb_host_ok) {
    usb_host_config_t host = {};
    host.skip_phy_setup = false;
    host.root_port_unpowered = false;
    host.intr_flags = ESP_INTR_FLAG_LEVEL1;
    esp_err_t err = usb_host_install(&host);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      Serial.printf("CSI: usb_host_install failed (%d) - TinyUSB device may own the PHY\n", (int)err);
      return false;
    }
    if (!s_usb_lib_task) {
      if (xTaskCreate(usb_lib_task, "usb_lib", 4096, nullptr, 2, &s_usb_lib_task) != pdPASS) {
        Serial.println("CSI: USB host lib task failed");
        return false;
      }
    }
    uvc_host_driver_config_t uvc_drv = {};
    uvc_drv.driver_task_stack_size = 6 * 1024;
    uvc_drv.driver_task_priority = 5;
    uvc_drv.xCoreID = tskNO_AFFINITY;
    uvc_drv.create_background_task = true;
    err = uvc_host_install(&uvc_drv);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      Serial.printf("CSI: uvc_host_install failed (%d)\n", (int)err);
      return false;
    }
    s_usb_host_ok = true;
  }

  uint16_t w = _cfg.uvc.width ? _cfg.uvc.width : 640;
  uint16_t h = _cfg.uvc.height ? _cfg.uvc.height : 480;
  auto fmt = (enum uvc_host_stream_format)_cfg.uvc.format;
  if (fmt == UVC_VS_FORMAT_DEFAULT) fmt = UVC_VS_FORMAT_MJPEG;

  uvc_host_stream_config_t sc = {};
  sc.event_cb = nullptr;
  sc.frame_cb = on_uvc_frame;
  sc.user_ctx = this;
  sc.usb.dev_addr = _cfg.uvc.dev_addr;
  sc.usb.vid = _cfg.uvc.vid;
  sc.usb.pid = _cfg.uvc.pid;
  sc.usb.uvc_stream_index = 0;
  sc.vs_format.h_res = w;
  sc.vs_format.v_res = h;
  sc.vs_format.fps = _cfg.uvc.fps;
  sc.vs_format.format = fmt;
  sc.advanced.number_of_frame_buffers = 2;
  sc.advanced.frame_size = 0;
  sc.advanced.frame_heap_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  sc.advanced.number_of_urbs = 3;
  sc.advanced.urb_size = 0;

  uvc_host_stream_hdl_t stream = nullptr;
  Serial.println("CSI: waiting for USB webcam (UVC host)...");
  if (uvc_host_stream_open(&sc, pdMS_TO_TICKS(8000), &stream) != ESP_OK) {
    Serial.println("CSI: no UVC device (plug a webcam, check USB host mode)");
    return false;
  }
  if (uvc_host_stream_start(stream) != ESP_OK) {
    uvc_host_stream_close(stream);
    Serial.println("CSI: UVC stream start failed");
    return false;
  }
  _uvc_stream = stream;
  _w = w;
  _h = h;
  _fps = _cfg.uvc.fps > 1 ? (uint8_t)_cfg.uvc.fps : 15;
  _use_isp = false;
  _in_fmt = (fmt == UVC_VS_FORMAT_YUY2) ? (uint8_t)ESP32P4_CAM_IN_YUV422
                                        : (uint8_t)ESP32P4_CAM_IN_RGB565;
  _cfg.pixel_format =
      (fmt == UVC_VS_FORMAT_YUY2) ? ESP32P4_PIXFORMAT_YUYV : ESP32P4_PIXFORMAT_JPEG;
  _started = true;
  Serial.printf("CSI: UVC host streaming %ux%u %s\n", _w, _h,
                esp32p4_pixformat_name(_cfg.pixel_format));
  return true;
}

bool ESP32P4_Camera::start_pipeline() {
  switch (_bus) {
    case ESP32P4_CAM_BUS_DVP: return init_dvp();
    case ESP32P4_CAM_BUS_SPI: return init_spi();
    case ESP32P4_CAM_BUS_UVC_HOST: return true;  // opened in init_uvc_host
    case ESP32P4_CAM_BUS_CSI:
    default: return init_csi_isp();
  }
}

void ESP32P4_Camera::teardown_uvc_host() {
  if (_uvc_stream) {
    auto h = (uvc_host_stream_hdl_t)_uvc_stream;
    uvc_host_stream_close(h);
    _uvc_stream = nullptr;
    if (_cam == h) _cam = nullptr;
    _started = false;
  }
}

bool ESP32P4_Camera::begin(esp32p4_board_t board) { return begin(esp32p4_cam_config_board(board)); }

bool ESP32P4_Camera::begin(const esp32p4_cam_config_t &cfg) {
  esp32p4_prefer_psram();
  if (_cam || _ldo || _isp || _uvc_stream) end();
  _cfg = cfg;
  _bus = cfg.bus;
  {
    const char *wire = (!_cfg.wire || _cfg.wire == &Wire) ? "Wire" : "Wire1";
    Serial.printf("CSI: begin sda=%d scl=%d %s  xclk=%d @%u  pwdn=%d rst=%d  ldo=%d/%dmV\n",
                  _cfg.sda, _cfg.scl, wire, _cfg.xclk, (unsigned)_cfg.xclk_hz, _cfg.pwdn,
                  _cfg.reset, _cfg.ldo_chan, _cfg.ldo_mv);
    Serial.printf("CSI: begin sensor_id=%d  framesize=%d  fmt=%s  fb=%u  bus=%u  lane_mbps=%d\n",
                  (int)_cfg.sensor, (int)_cfg.frame_size, esp32p4_pixformat_name(_cfg.pixel_format),
                  (unsigned)_cfg.fb_count, (unsigned)_cfg.bus, _cfg.lane_bit_rate_mbps);
  }
  if (_bus == ESP32P4_CAM_BUS_CSI && cfg.csi_id != 0) {
    Serial.printf("CSI: csi_id=%u rejected (P4 CSI host is 0)\n", (unsigned)cfg.csi_id);
    return false;
  }
  if (!claim_bus()) return false;
  Serial.printf("CSI: PSRAM %s  free=%u  bus=%s\n", esp32p4_psram_available() ? "ok" : "missing",
                (unsigned)esp32p4_psram_free_size(), busName());

  if (_bus == ESP32P4_CAM_BUS_UVC_HOST) {
    _w = _cfg.uvc.width ? _cfg.uvc.width : 640;
    _h = _cfg.uvc.height ? _cfg.uvc.height : 480;
    _use_isp = false;
    _cfg.pixel_format = (_cfg.uvc.format == 2) ? ESP32P4_PIXFORMAT_YUYV : ESP32P4_PIXFORMAT_JPEG;
    _sensor = ESP32P4_SENSOR_AUTO;
    _ops = nullptr;
    _addr = 0;
    if (!alloc_fbs()) return false;
    if (!init_uvc_host()) return false;
    Serial.printf("CSI: streaming %ux%u %s (USB host UVC)\n", _w, _h,
                  esp32p4_pixformat_name(_cfg.pixel_format));
    note_dual_live();
    return true;
  }

  if (_bus == ESP32P4_CAM_BUS_CSI) {
    if (!init_mipi_ldo()) return false;
  }
  if (!init_sensor()) return false;
  if (!alloc_fbs()) {
    if (_sensor == ESP32P4_SENSOR_OV5647 && _w >= 1920) {
      Serial.println("CSI: FB alloc failed at 1080p - retry 800x640");
      end();
      _cfg.frame_size = ESP32P4_FRAMESIZE_800X640;
      _cfg.lane_bit_rate_mbps = 200;
      return begin(_cfg);
    }
    if (_sensor == ESP32P4_SENSOR_OV5645 && _w >= 1920) {
      Serial.println("CSI: FB alloc failed at high-res - retry 1280x960 RGB565");
      end();
      _cfg.frame_size = ESP32P4_FRAMESIZE_SXGA;
      _cfg.lane_bit_rate_mbps = 448;
      return begin(_cfg);
    }
    return false;
  }
  if (!start_pipeline()) return false;
  if (!start_sensor_stream()) return false;
  if (_sensor == ESP32P4_SENSOR_OV5647) {
    if (afBegin()) Serial.printf("CSI: DW9714 AF @ 0x0C pos=%u\n", (unsigned)_af_pos);
  }
  if (!ensure_jpeg_encoder()) return false;
  if (_cfg.pixel_format == ESP32P4_PIXFORMAT_JPEG) {
    Serial.printf("CSI: streaming %ux%u JPEG (%s RGB565, q=%u)\n", _w, _h, busName(),
                  (unsigned)_jpeg_quality);
  } else {
    Serial.printf("CSI: streaming %ux%u %s\n", _w, _h, esp32p4_pixformat_name(_cfg.pixel_format));
  }
  note_dual_live();
  return true;
}

void ESP32P4_Camera::free_jpeg_encoder() {
  if (_jpeg_enc) {
    _jpeg_enc->end();
    delete _jpeg_enc;
    _jpeg_enc = nullptr;
  }
  if (_jpeg_scratch) {
    esp32p4_psram_free(_jpeg_scratch);
    _jpeg_scratch = nullptr;
    _jpeg_scratch_cap = 0;
  }
}

bool ESP32P4_Camera::ensure_jpeg_encoder() {
  if (_cfg.pixel_format != ESP32P4_PIXFORMAT_JPEG) {
    free_jpeg_encoder();
    return true;
  }
  if (!_jpeg_enc) _jpeg_enc = new ESP32P4_Jpeg();
  if (!_jpeg_enc->begin(_w, _h, _jpeg_quality)) {
    Serial.println("CSI: JPEG encoder begin failed");
    return false;
  }
  _jpeg_enc->setChroma(_jpeg_chroma);
  if (_jpeg_scratch) {
    esp32p4_psram_free(_jpeg_scratch);
    _jpeg_scratch = nullptr;
  }
  _jpeg_scratch_cap = esp32p4_pixformat_fb_bytes(ESP32P4_PIXFORMAT_RGB565, _w, _h);
  if (_jpeg_scratch_cap < 64 * 1024) _jpeg_scratch_cap = 64 * 1024;
  _jpeg_scratch = (uint8_t *)esp32p4_psram_alloc(_jpeg_scratch_cap);
  if (!_jpeg_scratch) {
    Serial.println("CSI: JPEG scratch alloc failed");
    return false;
  }
  return true;
}

void ESP32P4_Camera::teardown_pipeline() {
  teardown_uvc_host();
  if (_cam) {
    auto cam = (esp_cam_ctlr_handle_t)_cam;
    if (_started) esp_cam_ctlr_stop(cam);
    esp_cam_ctlr_disable(cam);
    esp_cam_ctlr_del(cam);
    _cam = nullptr;
    _started = false;
  }
  if (_isp_pipe) {
    _isp_pipe->end();
  }
  if (_isp) {
    esp_isp_del_processor((isp_proc_handle_t)_isp);
    _isp = nullptr;
  }
  free_jpeg_encoder();
  free_fbs();
}

void ESP32P4_Camera::end() {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (ops && ops->stream_off && _addr > 0) ops->stream_off((uint8_t)_addr);
  teardown_pipeline();
  delete _isp_pipe;
  _isp_pipe = nullptr;
  if (_ldo) {
    esp_ldo_release_channel((esp_ldo_channel_handle_t)_ldo);
    _ldo = nullptr;
  }
  if (_pool) {
    auto *p = ESP32P4_CamFbAccess::pool(this);
    if (p && p->sem) {
      vSemaphoreDelete(p->sem);
      p->sem = nullptr;
    }
    delete p;
    _pool = nullptr;
  }
  _af_ok = false;
  _af_pos = 0;
  release_bus();
}

camera_fb_t *ESP32P4_Camera::capture(uint32_t timeout_ms) {
  auto *p = ESP32P4_CamFbAccess::pool(this);
  if (!p || !p->sem || !_fb_n) return nullptr;
  if (!_cam && !_uvc_stream) return nullptr;
  const uint32_t t0 = millis();
  while ((millis() - t0) < timeout_ms) {
    uint32_t left = timeout_ms - (millis() - t0);
    if (!left) left = 1;
    if (xSemaphoreTake(p->sem, pdMS_TO_TICKS(left)) != pdTRUE) return nullptr;

    portDISABLE_INTERRUPTS();
    int idx = ready_pop(p);
    if (idx >= 0 && idx < (int)_fb_n) p->state[idx] = FB_HELD;
    portENABLE_INTERRUPTS();

    if (idx < 0 || idx >= (int)_fb_n) continue;

    // Keep the newest CSI frame; older queued buffers are live delay.
    for (;;) {
      if (xSemaphoreTake(p->sem, 0) != pdTRUE) break;
      portDISABLE_INTERRUPTS();
      int nidx = ready_pop(p);
      if (nidx >= 0 && nidx < (int)_fb_n) {
        p->state[idx] = FB_FREE;
        p->state[nidx] = FB_HELD;
        idx = nidx;
      }
      portENABLE_INTERRUPTS();
      if (nidx < 0) break;
    }

    esp32p4_psram_msync(_fb[idx].buf, _fb[idx].len ? _fb[idx].len : _fb_cap);
    _fb[idx].timestamp_us = (uint32_t)esp_timer_get_time();
    if (_bus == ESP32P4_CAM_BUS_UVC_HOST) {
      return &_fb[idx];
    }
    _fb[idx].width = _w;
    _fb[idx].height = _h;
    if (_isp_pipe) {
      CamSccbBind i2c_bind(_cfg.wire);
      _isp_pipe->process(this);
    }
    if (_cfg.pixel_format == ESP32P4_PIXFORMAT_JPEG) {
      if (!_jpeg_enc || !_jpeg_scratch) {
        portDISABLE_INTERRUPTS();
        p->state[idx] = FB_FREE;
        portENABLE_INTERRUPTS();
        return nullptr;
      }
      _jpeg_enc->setQuality(_jpeg_quality);
      const esp32p4_cam_pixformat_t src_fmt =
          (_in_fmt == (uint8_t)ESP32P4_CAM_IN_YUV422) ? ESP32P4_PIXFORMAT_YUV422
                                                      : ESP32P4_PIXFORMAT_RGB565;
      const size_t n = _jpeg_enc->encode(_fb[idx].buf, _w, _h, src_fmt, _jpeg_scratch,
                                         _jpeg_scratch_cap);
      if (!n) {
        portDISABLE_INTERRUPTS();
        p->state[idx] = FB_FREE;
        portENABLE_INTERRUPTS();
        continue;
      }
      memcpy(_fb[idx].buf, _jpeg_scratch, n);
      _fb[idx].len = n;
      _fb[idx].format = ESP32P4_PIXFORMAT_JPEG;
    } else {
      _fb[idx].format = _cfg.pixel_format;
      _fb[idx].len = (_cfg.pixel_format == ESP32P4_PIXFORMAT_GRAY8) ? ((size_t)_w * (size_t)_h)
                                                                   : _fb_cap;
      if (_cfg.pixel_format == ESP32P4_PIXFORMAT_YUYV && _bus != ESP32P4_CAM_BUS_UVC_HOST) {
        /* ISP / CSI YUV422 is UYVY; swap in-place to YUYV. Host UVC YUY2 is already YUYV. */
        uint8_t *p = _fb[idx].buf;
        const size_t n = _fb[idx].len;
        for (size_t i = 0; i + 3 < n; i += 4) {
          const uint8_t u = p[i], y0 = p[i + 1], v = p[i + 2], y1 = p[i + 3];
          p[i] = y0;
          p[i + 1] = u;
          p[i + 2] = y1;
          p[i + 3] = v;
        }
      }
    }
    return &_fb[idx];
  }
  return nullptr;
}

void ESP32P4_Camera::release(camera_fb_t *fb) {
  if (!fb || !fb->buf || !_fb_n) return;
  auto *p = ESP32P4_CamFbAccess::pool(this);
  if (!p) return;
  int idx = -1;
  for (uint8_t i = 0; i < _fb_n; i++) {
    if (_fb[i].buf == fb->buf) {
      idx = (int)i;
      break;
    }
  }
  if (idx < 0) return;
  portDISABLE_INTERRUPTS();
  if (p->state[idx] == FB_HELD) p->state[idx] = FB_FREE;
  portENABLE_INTERRUPTS();
}

bool ESP32P4_Camera::setTestPattern(bool enable) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_test_pattern || _addr <= 0) return false;
  _cfg.test_pattern = enable;
  return ops->set_test_pattern((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::sync_isp_bayer_for_flip() {
  // OV5647: sensor flip rotates Bayer; ISP must follow (Linux ov5647_get_mbus_code).
  if (!_isp || _sensor != ESP32P4_SENSOR_OV5647) return true;
  bool hm = false, vf = false;
  if (!getHMirror(&hm) || !getVFlip(&vf)) return false;
  static const color_raw_element_order_t kOrder[4] = {
      COLOR_RAW_ELEMENT_ORDER_GBRG,  // h=0 v=0  SGBRG
      COLOR_RAW_ELEMENT_ORDER_BGGR,  // h=1 v=0  SBGGR
      COLOR_RAW_ELEMENT_ORDER_RGGB,  // h=0 v=1  SRGGB
      COLOR_RAW_ELEMENT_ORDER_GRBG,  // h=1 v=1  SGRBG
  };
  const int idx = (hm ? 1 : 0) | ((vf ? 1 : 0) << 1);
  isp_ll_set_bayer_mode(ISP_LL_GET_HW(0), kOrder[idx]);
  return true;
}

bool ESP32P4_Camera::setHMirror(bool enable) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_hmirror || _addr <= 0) return false;
  if (!ops->set_hmirror((uint8_t)_addr, enable)) return false;
  return sync_isp_bayer_for_flip();
}

bool ESP32P4_Camera::setVFlip(bool enable) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_vflip || _addr <= 0) return false;
  if (!ops->set_vflip((uint8_t)_addr, enable)) return false;
  return sync_isp_bayer_for_flip();
}

bool ESP32P4_Camera::setAEC(bool enable) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_aec || _addr <= 0) return false;
  if (enable && _isp_pipe) _isp_pipe->setAe(false);
  return ops->set_aec((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setAGC(bool enable) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_agc || _addr <= 0) return false;
  return ops->set_agc((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setExposure(uint16_t lines) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_exposure || _addr <= 0) return false;
  if (ops->set_aec && !ops->set_aec((uint8_t)_addr, false)) return false;
  return ops->set_exposure((uint8_t)_addr, lines);
}

bool ESP32P4_Camera::setGain(uint16_t gain) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_gain || _addr <= 0) return false;
  if (ops->set_agc && !ops->set_agc((uint8_t)_addr, false)) return false;
  return ops->set_gain((uint8_t)_addr, gain);
}

bool ESP32P4_Camera::setGainCeiling(uint16_t ceiling) {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_gainceiling || _addr <= 0) return false;
  return ops->set_gainceiling((uint8_t)_addr, ceiling);
}

bool ESP32P4_Camera::getHMirror(bool *out) const {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_hmirror || _addr <= 0) return false;
  return ops->get_hmirror((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getVFlip(bool *out) const {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_vflip || _addr <= 0) return false;
  return ops->get_vflip((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getAEC(bool *out) const {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_aec || _addr <= 0) return false;
  return ops->get_aec((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getAGC(bool *out) const {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_agc || _addr <= 0) return false;
  return ops->get_agc((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getExposure(uint16_t *lines) const {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_exposure || _addr <= 0) return false;
  return ops->get_exposure((uint8_t)_addr, lines);
}

bool ESP32P4_Camera::getGain(uint16_t *gain) const {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_gain || _addr <= 0) return false;
  return ops->get_gain((uint8_t)_addr, gain);
}

bool ESP32P4_Camera::getGainCeiling(uint16_t *ceiling) const {
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_gainceiling || _addr <= 0) return false;
  return ops->get_gainceiling((uint8_t)_addr, ceiling);
}

bool ESP32P4_Camera::startCapture() {
  if (_uvc_stream) {
    if (_started) return true;
    if (uvc_host_stream_start((uvc_host_stream_hdl_t)_uvc_stream) != ESP_OK) return false;
    _started = true;
    return true;
  }
  if (!_cam) return false;
  if (_started) return true;
  if (esp_cam_ctlr_start((esp_cam_ctlr_handle_t)_cam) != ESP_OK) return false;
  _started = true;
  return true;
}

bool ESP32P4_Camera::stopCapture() {
  if (_uvc_stream) {
    if (!_started) return true;
    if (uvc_host_stream_stop((uvc_host_stream_hdl_t)_uvc_stream) != ESP_OK) return false;
    _started = false;
    return true;
  }
  if (!_cam || !_started) return true;
  if (esp_cam_ctlr_stop((esp_cam_ctlr_handle_t)_cam) != ESP_OK) return false;
  _started = false;
  return true;
}

bool ESP32P4_Camera::setFormat(esp32p4_cam_pixformat_t fmt) {
  if (_bus == ESP32P4_CAM_BUS_UVC_HOST) return false;
  fmt = resolve_format(fmt);
  if (fmt == _cfg.pixel_format && _cam) return true;
  _cfg.pixel_format = fmt;
  if (!_cam) return true;
  CamSccbBind i2c_bind(_cfg.wire);
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (ops && ops->stream_off && _addr > 0) ops->stream_off((uint8_t)_addr);
  teardown_pipeline();
  if (!alloc_fbs()) return false;
  if (!start_pipeline()) return false;
  if (!start_sensor_stream()) return false;
  return ensure_jpeg_encoder();
}

bool ESP32P4_Camera::setExposureTime(int32_t units_100us) {
  if (units_100us < 1) units_100us = 1;
  const uint32_t us = (uint32_t)units_100us * 100u;
  uint32_t line_us = _line_us ? _line_us : cam_line_us(_fps, _h);
  if (line_us < 8) line_us = 8;
  uint32_t lines = (us + line_us / 2) / line_us;
  if (lines < 4) lines = 4;
  if (lines > 65535) lines = 65535;
  return setExposure((uint16_t)lines);
}

bool ESP32P4_Camera::getExposureTime(int32_t *units_100us) const {
  if (!units_100us) return false;
  uint16_t lines = 0;
  if (!getExposure(&lines)) return false;
  uint32_t line_us = _line_us ? _line_us : cam_line_us(_fps, _h);
  uint32_t us = (uint32_t)lines * line_us;
  *units_100us = (int32_t)((us + 50u) / 100u);
  if (*units_100us < 1) *units_100us = 1;
  return true;
}

bool ESP32P4_Camera::setAeTarget(uint8_t luma) {
  if (_isp_pipe) _isp_pipe->setAeTarget(luma);
  return true;
}

uint8_t ESP32P4_Camera::aeTarget() const {
  return _isp_pipe ? _isp_pipe->aeTarget() : 80;
}

bool ESP32P4_Camera::setAeEvBias(int half_stops) {
  if (_isp_pipe) _isp_pipe->setEvBias(half_stops);
  return true;
}

int ESP32P4_Camera::aeEvBias() const {
  return _isp_pipe ? _isp_pipe->evBias() : 0;
}

bool ESP32P4_Camera::setJpegQuality(uint8_t q) {
  _jpeg_quality = q < 1 ? 1 : (q > 100 ? 100 : q);
  if (_jpeg_enc) _jpeg_enc->setQuality(_jpeg_quality);
  return true;
}

bool ESP32P4_Camera::setJpegChroma(esp32p4_jpeg_chroma_t c) {
  _jpeg_chroma = c;
  if (_jpeg_enc) _jpeg_enc->setChroma(c);
  return true;
}

bool ESP32P4_Camera::setBrightness(int8_t v) {
  return _isp_pipe ? _isp_pipe->setBrightness(v) : false;
}
bool ESP32P4_Camera::setContrast(uint8_t v) {
  return _isp_pipe ? _isp_pipe->setContrast(v) : false;
}
bool ESP32P4_Camera::setSaturation(uint8_t v) {
  return _isp_pipe ? _isp_pipe->setSaturation(v) : false;
}
bool ESP32P4_Camera::setHue(uint16_t deg) { return _isp_pipe ? _isp_pipe->setHue(deg) : false; }
bool ESP32P4_Camera::setAwb(bool on) { return _isp_pipe ? _isp_pipe->setAwb(on) : false; }
bool ESP32P4_Camera::setIspAe(bool on) { return _isp_pipe ? _isp_pipe->setAe(on) : false; }
bool ESP32P4_Camera::setRedBalance(int32_t v1024) {
  return _isp_pipe ? _isp_pipe->setRedBalance(v1024) : false;
}
bool ESP32P4_Camera::setBlueBalance(int32_t v1024) {
  return _isp_pipe ? _isp_pipe->setBlueBalance(v1024) : false;
}
bool ESP32P4_Camera::setSharpness(uint8_t v) {
  return _isp_pipe ? _isp_pipe->setSharpness(v) : false;
}
bool ESP32P4_Camera::setDenoise(uint8_t v) {
  return _isp_pipe ? _isp_pipe->setDenoise(v) : false;
}
bool ESP32P4_Camera::ispReady() const { return _isp_pipe && _isp_pipe->ready(); }
float ESP32P4_Camera::ispLuma() const { return _isp_pipe ? _isp_pipe->lastLuma() : 0.f; }
float ESP32P4_Camera::ispEnvLuma() const { return _isp_pipe ? _isp_pipe->envLuma() : 0.f; }
int8_t ESP32P4_Camera::brightness() const { return _isp_pipe ? _isp_pipe->brightness() : 0; }
uint8_t ESP32P4_Camera::contrast() const { return _isp_pipe ? _isp_pipe->contrast() : 128; }
uint8_t ESP32P4_Camera::saturation() const { return _isp_pipe ? _isp_pipe->saturation() : 128; }
uint16_t ESP32P4_Camera::hue() const { return _isp_pipe ? _isp_pipe->hue() : 0; }
uint8_t ESP32P4_Camera::sharpness() const { return _isp_pipe ? _isp_pipe->sharpness() : 128; }
uint8_t ESP32P4_Camera::denoise() const { return _isp_pipe ? _isp_pipe->denoise() : 0; }

static const uint8_t kDw9714Addr = 0x0C;
static const uint16_t kDw9714ProtOff = 0xeca3;
static const uint16_t kDw9714ProtOn = 0xdc51;

bool ESP32P4_Camera::afBegin() {
  CamSccbBind i2c_bind(_cfg.wire);
  _af_ok = false;
  _af_pos = 0;
  if (!esp32p4_sccb_ping(kDw9714Addr)) return false;
  /* Espressif LSC mode: mclk=1, tsrc=3 (Apache-2.0 dw9714_settings.h). */
  const uint16_t init[] = {kDw9714ProtOff, (uint16_t)(0xA104 | 0x1), (uint16_t)(0xF200 | (0x3 << 3)),
                           kDw9714ProtOn};
  for (uint16_t w : init) {
    if (!esp32p4_sccb_write_be16(kDw9714Addr, w)) return false;
  }
  delay(12);
  _af_ok = true;
  return setAfPosition(0);
}

bool ESP32P4_Camera::setAfPosition(uint16_t pos) {
  CamSccbBind i2c_bind(_cfg.wire);
  if (!_af_ok) return false;
  if (pos > 1023) pos = 1023;
  /* LSC_SET_CODE: (pos << 4) | (s32=3 << 2) | mclk=1 */
  const uint16_t code = (uint16_t)((pos << 4) | (3u << 2) | 1u);
  if (!esp32p4_sccb_write_be16(kDw9714Addr, code)) return false;
  _af_pos = pos;
  return true;
}

uint32_t ESP32P4_Camera::afScore() { return _isp_pipe ? _isp_pipe->afScore() : 0; }

bool ESP32P4_Camera::afScan(uint16_t min_pos, uint16_t max_pos) {
  if (!_af_ok && !afBegin()) return false;
  if (!_isp_pipe || !_isp_pipe->ready()) {
    Serial.println("CSI: AF scan needs ISP (RAW CSI)");
    return false;
  }
  if (!_isp_pipe->startAf()) {
    Serial.println("CSI: ISP AF controller failed");
    return false;
  }
  if (max_pos == 0) max_pos = _isp_pipe->afMaxPos();
  if (max_pos > 1023) max_pos = 1023;
  if (min_pos > max_pos) min_pos = 0;
  auto run = [&](uint16_t lo, uint16_t hi, uint8_t n, uint16_t *best, uint32_t *best_sc) -> bool {
    if (n < 2) n = 2;
    for (uint8_t i = 0; i < n; i++) {
      const uint16_t pos = (uint16_t)(lo + (uint32_t)(hi - lo) * i / (uint32_t)(n - 1));
      if (!setAfPosition(pos)) return false;
      delay(25);
      const uint32_t sc = _isp_pipe->afScore();
      if (sc > *best_sc) {
        *best_sc = sc;
        *best = pos;
      }
    }
    return true;
  };
  uint16_t best = min_pos;
  uint32_t best_sc = 0;
  if (!run(min_pos, max_pos, 10, &best, &best_sc)) return false;
  uint16_t span = (uint16_t)((max_pos - min_pos) / 10);
  if (span < 8) span = 8;
  const uint16_t lo = (best > min_pos + span) ? (uint16_t)(best - span) : min_pos;
  const uint16_t hi = (uint16_t)((best + span > max_pos) ? max_pos : best + span);
  uint16_t fine = best;
  uint32_t fine_sc = 0;
  if (!run(lo, hi, 10, &fine, &fine_sc)) return false;
  if (fine_sc >= best_sc) best = fine;
  if (!setAfPosition(best)) return false;
  Serial.printf("CSI: AF scan pos=%u score=%u\n", (unsigned)best,
                (unsigned)(fine_sc > best_sc ? fine_sc : best_sc));
  return true;
}

bool ESP32P4_Camera::setAntiFlicker(uint8_t hz) {
  if (!_isp_pipe) return false;
  _isp_pipe->setAntiFlicker(hz);
  return true;
}

uint8_t ESP32P4_Camera::antiFlicker() const {
  return _isp_pipe ? _isp_pipe->antiFlicker() : 0;
}

static uint16_t sensor_pid(esp32p4_cam_sensor_t s) {
  switch (s) {
    case ESP32P4_SENSOR_SC2336: return 0xcb3a;
    case ESP32P4_SENSOR_OV5647: return 0x5647;
    case ESP32P4_SENSOR_OV9281: return 0x9281;
    case ESP32P4_SENSOR_SC202CS: return 0xeb52;
    case ESP32P4_SENSOR_SC1346: return 0xda4d;
    case ESP32P4_SENSOR_SC035HGS: return 0x0031;
    case ESP32P4_SENSOR_IMX708: return 0x0708;
    case ESP32P4_SENSOR_OV5645: return 0x5645;
    case ESP32P4_SENSOR_OV2710: return 0x2710;
    case ESP32P4_SENSOR_SC030IOT: return 0x9a46;
    case ESP32P4_SENSOR_OS02N10: return 0x534e;
    case ESP32P4_SENSOR_OS04C10: return 0x5304;
    case ESP32P4_SENSOR_STI2250: return 0x2250;
    case ESP32P4_SENSOR_MIRA220: return 0x0130;
    case ESP32P4_SENSOR_IMX219: return 0x0219;
    case ESP32P4_SENSOR_IMX477: return 0x0477;
    case ESP32P4_SENSOR_IMX462: return 0x0462;
    case ESP32P4_SENSOR_IMX335: return 0x0335;
    case ESP32P4_SENSOR_IMX415: return 0x0415;
    case ESP32P4_SENSOR_IMX296: return 0x0296;
    case ESP32P4_SENSOR_GC2145: return 0x2145;
    case ESP32P4_SENSOR_SC2331: return 0xcb5c;
    case ESP32P4_SENSOR_GC2607: return 0x2607;
    case ESP32P4_SENSOR_OV5640: return 0x5640;
    case ESP32P4_SENSOR_LT6911: return 0x2ada;
    case ESP32P4_SENSOR_OV2640: return 0x2642;
    case ESP32P4_SENSOR_SP0A39: return 0x0a39;
    default: return 0;
  }
}

bool ESP32P4_Camera::sensorIoctl(uint32_t cmd, void *arg, size_t size) {
  const uint32_t id = ESP_CAM_SENSOR_IOC_GET_ID(cmd);
  CamSccbBind i2c_bind(_cfg.wire);
  if (id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_G_CHIP_ID)) {
    if (!arg || size < sizeof(esp_cam_sensor_id_t)) return false;
    auto *cid = (esp_cam_sensor_id_t *)arg;
    memset(cid, 0, sizeof(*cid));
    cid->pid = sensor_pid(_sensor);
    cid->midh = (uint8_t)(cid->pid >> 8);
    cid->midl = (uint8_t)cid->pid;
    return cid->pid != 0;
  }
  if (id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_S_STREAM)) {
    int on = 1;
    if (arg && size >= sizeof(int)) on = *(int *)arg;
    return on ? startCapture() : stopCapture();
  }
  if (id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_S_TEST_PATTERN)) {
    int on = 0;
    if (arg && size >= sizeof(int)) on = *(int *)arg;
    return setTestPattern(on != 0);
  }
  if (id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_S_GAIN)) {
    uint8_t g = 16;
    if (arg && size >= 1) g = *(uint8_t *)arg;
    return setGain(g);
  }
  if (id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_S_REG)) {
    if (!arg || size < sizeof(esp_cam_sensor_reg_val_t) || _addr <= 0) return false;
    auto *r = (esp_cam_sensor_reg_val_t *)arg;
    if (r->regaddr > 0xff) return esp32p4_sccb_write8((uint8_t)_addr, (uint16_t)r->regaddr, (uint8_t)r->value);
    return esp32p4_sccb_write8_reg8((uint8_t)_addr, (uint8_t)r->regaddr, (uint8_t)r->value);
  }
  if (id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_G_REG)) {
    if (!arg || size < sizeof(esp_cam_sensor_reg_val_t) || _addr <= 0) return false;
    auto *r = (esp_cam_sensor_reg_val_t *)arg;
    uint8_t v = 0;
    bool ok = (r->regaddr > 0xff)
                  ? esp32p4_sccb_read8((uint8_t)_addr, (uint16_t)r->regaddr, &v)
                  : esp32p4_sccb_read8_reg8((uint8_t)_addr, (uint8_t)r->regaddr, &v);
    if (ok) r->value = v;
    return ok;
  }
  if (id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_HW_RESET) ||
      id == ESP_CAM_SENSOR_IOC_GET_ID(ESP_CAM_SENSOR_IOC_SW_RESET)) {
    return true;
  }
  return false;
}

