#include "cam/ESP32P4_Camera.h"

#include <Wire.h>
#include <string.h>

#include "cam/esp32p4_cam_sensor_ops.h"
#include "driver/isp.h"
#include "driver/isp_demosaic.h"
#include "driver/ledc.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_ldo_regulator.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "hal/color_types.h"
#include "hal/isp_ll.h"
#include "mem/ESP32P4_Psram.h"

static SemaphoreHandle_t s_frame_sem = nullptr;
static esp_cam_ctlr_trans_t s_trans[ESP32P4_CAM_FB_MAX]{};
static uint8_t *s_fb_ptr[ESP32P4_CAM_FB_MAX]{};
static volatile uint8_t s_fb_state[ESP32P4_CAM_FB_MAX]{};  // 0=free 1=csi 2=ready 3=held
static volatile int s_ready_q[ESP32P4_CAM_FB_MAX]{};
static volatile int s_ready_r = 0;
static volatile int s_ready_w = 0;
static volatile int s_ready_n = 0;
static volatile uint32_t s_new_trans_count = 0;
static volatile uint32_t s_done_count = 0;
static volatile uint32_t s_drop_count = 0;
static uint8_t s_fb_n = 1;

enum : uint8_t { FB_FREE = 0, FB_CSI = 1, FB_READY = 2, FB_HELD = 3 };

static int IRAM_ATTR fb_index_from_buf(const void *buf) {
  for (uint8_t i = 0; i < s_fb_n; i++) {
    if (s_fb_ptr[i] == buf) return (int)i;
  }
  return -1;
}

static bool IRAM_ATTR ready_push(int idx) {
  if (s_ready_n >= (int)s_fb_n) return false;
  s_ready_q[s_ready_w] = idx;
  s_ready_w = (s_ready_w + 1) % (s_fb_n ? s_fb_n : 1);
  s_ready_n++;
  return true;
}

static int IRAM_ATTR ready_pop(void) {
  if (s_ready_n <= 0) return -1;
  int idx = s_ready_q[s_ready_r];
  s_ready_r = (s_ready_r + 1) % (s_fb_n ? s_fb_n : 1);
  s_ready_n--;
  return idx;
}

static bool IRAM_ATTR on_get_new_trans(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *trans, void *) {
  s_new_trans_count++;
  // Prefer a free buffer so CSI never writes into a frame the app is still using.
  for (uint8_t i = 0; i < s_fb_n; i++) {
    if (s_fb_state[i] == FB_FREE) {
      s_fb_state[i] = FB_CSI;
      *trans = s_trans[i];
      return false;
    }
  }
  // All buffers busy (app slower than sensor): drop oldest ready frame and reuse it.
  if (s_ready_n > 0) {
    int idx = ready_pop();
    if (idx >= 0 && idx < (int)s_fb_n) {
      s_drop_count++;
      s_fb_state[idx] = FB_CSI;
      *trans = s_trans[idx];
      return false;
    }
  }
  // Last resort: overwrite an in-flight CSI slot (should be rare with 3 FBs).
  for (uint8_t i = 0; i < s_fb_n; i++) {
    if (s_fb_state[i] == FB_CSI) {
      s_drop_count++;
      *trans = s_trans[i];
      return false;
    }
  }
  *trans = s_trans[0];
  s_drop_count++;
  return false;
}

static bool IRAM_ATTR on_trans_finished(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *trans, void *) {
  s_done_count++;
  int idx = trans ? fb_index_from_buf(trans->buffer) : -1;
  if (idx < 0 || idx >= (int)s_fb_n) {
    BaseType_t woken = pdFALSE;
    if (s_frame_sem) xSemaphoreGiveFromISR(s_frame_sem, &woken);
    return woken == pdTRUE;
  }
  // If app never drained ready queue, drop this frame back to free instead of growing backlog.
  if (s_ready_n >= (int)s_fb_n - 1) {
    // Keep at most one ready frame waiting; drop older by replacing.
    int old = ready_pop();
    if (old >= 0 && old < (int)s_fb_n) {
      s_fb_state[old] = FB_FREE;
      s_drop_count++;
    }
  }
  s_fb_state[idx] = FB_READY;
  ready_push(idx);
  BaseType_t woken = pdFALSE;
  if (s_frame_sem) xSemaphoreGiveFromISR(s_frame_sem, &woken);
  return woken == pdTRUE;
}

static bool start_xclk_gpio(int gpio, uint32_t hz) {
  ledc_timer_config_t timer = {};
  timer.speed_mode = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = LEDC_TIMER_1_BIT;
  timer.timer_num = LEDC_TIMER_0;
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
  ch.channel = LEDC_CHANNEL_0;
  ch.timer_sel = LEDC_TIMER_0;
  ch.duty = 1;
  ch.hpoint = 0;
  if (ledc_channel_config(&ch) != ESP_OK) return false;
  Serial.printf("CSI: XCLK %u Hz on GPIO%d\n", (unsigned)timer.freq_hz, gpio);
  return true;
}

esp32p4_cam_config_t esp32p4_cam_config_default() {
  return esp32p4_cam_config_board(ESP32P4_BOARD_GUITION_M3);
}

esp32p4_cam_config_t esp32p4_cam_config_board(esp32p4_board_t board) {
  esp32p4_cam_config_t c{};
  c.sda = 7;
  c.scl = 8;
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

uint32_t ESP32P4_Camera::newTransCount() const { return s_new_trans_count; }
uint32_t ESP32P4_Camera::doneCount() const { return s_done_count; }
bool ESP32P4_Camera::psramOk() const { return esp32p4_psram_available(); }

bool ESP32P4_Camera::probe_sensor() {
  uint8_t a = 0;
  const esp32p4_cam_sensor_ops_t *ops = esp32p4_cam_sensor_probe(_cfg.sensor, &a);
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
  if (_cfg.xclk >= 0) {
    if (!start_xclk_gpio(_cfg.xclk, _cfg.xclk_hz ? _cfg.xclk_hz : 24000000)) return false;
    delay(20);
  }

  Wire.begin(_cfg.sda, _cfg.scl);
  Wire.setClock(100000);
  delay(20);

  Serial.printf("CSI: I2C scan SDA=%d SCL=%d  registry=%u sensors\n", _cfg.sda, _cfg.scl,
                (unsigned)esp32p4_cam_sensor_registry_count());
  int found = 0;
  for (int a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  ack 0x%02X\n", a);
      found++;
    }
  }
  if (!found) Serial.println("  (no devices — check CSI ribbon)");

  if (!probe_sensor()) {
    Serial.println("CSI: no supported MIPI sensor with a usable mode");
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
  _lanes = mode.lanes ? mode.lanes : 2;
  _bayer = (uint8_t)mode.bayer;
  _raw8 = (mode.in_fmt == ESP32P4_CAM_IN_RAW8);
  _use_isp = (mode.in_fmt == ESP32P4_CAM_IN_RAW8 || mode.in_fmt == ESP32P4_CAM_IN_RAW10);
  if (_cfg.lane_bit_rate_mbps <= 0) _cfg.lane_bit_rate_mbps = mode.lane_mbps > 0 ? mode.lane_mbps : 400;
  _cfg.pixel_format = ESP32P4_PIXFORMAT_RGB565;
  Serial.printf("CSI: %s mode %s  %ux%u  lanes=%u  %d Mbps/lane  isp=%d\n", ops->name,
                mode.name ? mode.name : "?", _w, _h, (unsigned)_lanes, _cfg.lane_bit_rate_mbps,
                _use_isp ? 1 : 0);
  return true;
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
  _fb_cap = (size_t)_w * (size_t)_h * 2;
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
    s_trans[i].buffer = buf;
    s_trans[i].buflen = _fb_cap;
    s_fb_ptr[i] = buf;
    s_fb_state[i] = FB_FREE;
  }
  for (uint8_t i = _fb_n; i < ESP32P4_CAM_FB_MAX; i++) {
    s_fb_ptr[i] = nullptr;
    s_fb_state[i] = FB_FREE;
    s_trans[i] = {};
  }
  s_fb_n = _fb_n;
  s_ready_r = 0;
  s_ready_w = 0;
  s_ready_n = 0;
  s_drop_count = 0;
  Serial.printf("CSI: %u PSRAM framebuffers x %u bytes (PSRAM free=%u)\n", _fb_n, (unsigned)_fb_cap,
                (unsigned)esp32p4_psram_free_size());
  return true;
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
  const bool sensor_rgb565 = !_use_isp;  // sensor already outputs RGB565

  if (_use_isp && _cfg.pixel_format == ESP32P4_PIXFORMAT_RGB565) {
    esp_isp_processor_cfg_t isp_cfg = {};
    // Higher ISP clock needed for 1080p RAW10 demosaic; 80 MHz overflows.
    isp_cfg.clk_hz = (_w >= 1600) ? (160 * 1000 * 1000) : (80 * 1000 * 1000);
    isp_cfg.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
    isp_cfg.input_data_color_type = _raw8 ? ISP_COLOR_RAW8 : ISP_COLOR_RAW10;
    isp_cfg.output_data_color_type = ISP_COLOR_RGB565;
    isp_cfg.has_line_start_packet = false;
    isp_cfg.has_line_end_packet = false;
    isp_cfg.h_res = _w;
    isp_cfg.v_res = _h;
    isp_cfg.bayer_order = bayer_to_idf(_bayer);
    isp_proc_handle_t isp = nullptr;
    if (esp_isp_new_processor(&isp_cfg, &isp) != ESP_OK) return false;
    if (esp_isp_enable(isp) != ESP_OK) return false;
    esp_isp_demosaic_config_t demosaic = {};
    demosaic.padding_mode = ISP_DEMOSAIC_EDGE_PADDING_MODE_SRND_DATA;
    if (esp_isp_demosaic_configure(isp, &demosaic) != ESP_OK) return false;
    if (esp_isp_demosaic_enable(isp) != ESP_OK) return false;
    _isp = isp;
    Serial.printf("CSI: ISP RAW->RGB565 + demosaic (clk=%u MHz bayer=%u)\n",
                  (unsigned)(isp_cfg.clk_hz / 1000000), (unsigned)_bayer);
  }

  esp_cam_ctlr_csi_config_t csi_cfg = {};
  csi_cfg.ctlr_id = 0;
  csi_cfg.h_res = _w;
  csi_cfg.v_res = _h;
  csi_cfg.data_lane_num = lanes;
  csi_cfg.lane_bit_rate_mbps = bitrate;
  if (sensor_rgb565) {
    csi_cfg.input_data_color_type = CAM_CTLR_COLOR_RGB565;
    csi_cfg.output_data_color_type = CAM_CTLR_COLOR_RGB565;
  } else {
    csi_cfg.input_data_color_type = _raw8 ? CAM_CTLR_COLOR_RAW8 : CAM_CTLR_COLOR_RAW10;
    csi_cfg.output_data_color_type = _raw8 ? CAM_CTLR_COLOR_RAW8 : CAM_CTLR_COLOR_RAW10;
  }
  csi_cfg.queue_items = (_w >= 1600) ? 2 : 1;
  csi_cfg.byte_swap_en = false;

  esp_cam_ctlr_handle_t cam = nullptr;
  if (esp_cam_new_csi_ctlr(&csi_cfg, &cam) != ESP_OK) return false;
  _cam = cam;

  esp_cam_ctlr_evt_cbs_t cbs = {};
  cbs.on_get_new_trans = on_get_new_trans;
  cbs.on_trans_finished = on_trans_finished;
  if (esp_cam_ctlr_register_event_callbacks(cam, &cbs, s_trans) != ESP_OK) return false;
  if (esp_cam_ctlr_enable(cam) != ESP_OK) return false;
  if (esp_cam_ctlr_start(cam) != ESP_OK) return false;
  _started = true;
  Serial.printf("CSI: controller started %ux%u  %u-lane @ %d Mbps/lane\n", _w, _h, (unsigned)lanes,
                bitrate);
  return true;
}

bool ESP32P4_Camera::start_sensor_stream() {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->stream_on) return false;
  if (!ops->stream_on((uint8_t)_addr)) return false;
  if (_cfg.test_pattern && ops->set_test_pattern) {
    ops->set_test_pattern((uint8_t)_addr, true);
  }
  delay(200);
  return true;
}

bool ESP32P4_Camera::begin(esp32p4_board_t board) { return begin(esp32p4_cam_config_board(board)); }

bool ESP32P4_Camera::begin(const esp32p4_cam_config_t &cfg) {
  _cfg = cfg;
  s_new_trans_count = 0;
  s_done_count = 0;
  if (!s_frame_sem) {
    s_frame_sem = xSemaphoreCreateBinary();
    if (!s_frame_sem) return false;
  }
  while (xSemaphoreTake(s_frame_sem, 0) == pdTRUE) {
  }
  Serial.printf("CSI: PSRAM %s  free=%u\n", esp32p4_psram_available() ? "ok" : "missing",
                (unsigned)esp32p4_psram_free_size());
  if (!init_mipi_ldo()) return false;
  if (!init_sensor()) return false;
  if (!alloc_fbs()) {
    // 1080p may be too large — fall back to 800x640 once.
    if (_sensor == ESP32P4_SENSOR_OV5647 && _w >= 1920) {
      Serial.println("CSI: FB alloc failed at 1080p — retry 800x640");
      end();
      _cfg.frame_size = ESP32P4_FRAMESIZE_800X640;
      _cfg.lane_bit_rate_mbps = 200;
      return begin(_cfg);
    }
    return false;
  }
  if (!init_csi_isp()) return false;
  if (!start_sensor_stream()) return false;
  Serial.printf("CSI: streaming %ux%u RGB565\n", _w, _h);
  return true;
}

void ESP32P4_Camera::end() {
  if (_cam) {
    auto cam = (esp_cam_ctlr_handle_t)_cam;
    if (_started) esp_cam_ctlr_stop(cam);
    esp_cam_ctlr_disable(cam);
    esp_cam_ctlr_del(cam);
    _cam = nullptr;
    _started = false;
  }
  if (_isp) {
    esp_isp_del_processor((isp_proc_handle_t)_isp);
    _isp = nullptr;
  }
  for (uint8_t i = 0; i < _fb_n; i++) {
    esp32p4_psram_free(_fb[i].buf);
    _fb[i] = {};
    s_trans[i] = {};
  }
  _fb_n = 0;
  if (_ldo) {
    esp_ldo_release_channel((esp_ldo_channel_handle_t)_ldo);
    _ldo = nullptr;
  }
}

camera_fb_t *ESP32P4_Camera::capture(uint32_t timeout_ms) {
  if (!_cam || !s_frame_sem || !_fb_n) return nullptr;
  const uint32_t t0 = millis();
  while ((millis() - t0) < timeout_ms) {
    uint32_t left = timeout_ms - (millis() - t0);
    if (!left) left = 1;
    if (xSemaphoreTake(s_frame_sem, pdMS_TO_TICKS(left)) != pdTRUE) return nullptr;

    portDISABLE_INTERRUPTS();
    int idx = ready_pop();
    if (idx >= 0 && idx < (int)_fb_n) s_fb_state[idx] = FB_HELD;
    portENABLE_INTERRUPTS();

    // Semaphore can fire for a frame that was later dropped — retry.
    if (idx < 0 || idx >= (int)_fb_n) continue;

    esp32p4_psram_msync(_fb[idx].buf, _fb_cap);
    _fb[idx].len = _fb_cap;
    _fb[idx].timestamp_us = (uint32_t)esp_timer_get_time();
    return &_fb[idx];
  }
  return nullptr;
}

void ESP32P4_Camera::release(camera_fb_t *fb) {
  if (!fb || !fb->buf || !_fb_n) return;
  int idx = -1;
  for (uint8_t i = 0; i < _fb_n; i++) {
    if (_fb[i].buf == fb->buf) {
      idx = (int)i;
      break;
    }
  }
  if (idx < 0) return;
  portDISABLE_INTERRUPTS();
  if (s_fb_state[idx] == FB_HELD) s_fb_state[idx] = FB_FREE;
  portENABLE_INTERRUPTS();
}

bool ESP32P4_Camera::setTestPattern(bool enable) {
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
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_hmirror || _addr <= 0) return false;
  if (!ops->set_hmirror((uint8_t)_addr, enable)) return false;
  return sync_isp_bayer_for_flip();
}

bool ESP32P4_Camera::setVFlip(bool enable) {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_vflip || _addr <= 0) return false;
  if (!ops->set_vflip((uint8_t)_addr, enable)) return false;
  return sync_isp_bayer_for_flip();
}

bool ESP32P4_Camera::setAEC(bool enable) {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_aec || _addr <= 0) return false;
  return ops->set_aec((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setAGC(bool enable) {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_agc || _addr <= 0) return false;
  return ops->set_agc((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setExposure(uint16_t lines) {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_exposure || _addr <= 0) return false;
  if (ops->set_aec && !ops->set_aec((uint8_t)_addr, false)) return false;
  return ops->set_exposure((uint8_t)_addr, lines);
}

bool ESP32P4_Camera::setGain(uint16_t gain) {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_gain || _addr <= 0) return false;
  if (ops->set_agc && !ops->set_agc((uint8_t)_addr, false)) return false;
  return ops->set_gain((uint8_t)_addr, gain);
}

bool ESP32P4_Camera::setGainCeiling(uint16_t ceiling) {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->set_gainceiling || _addr <= 0) return false;
  return ops->set_gainceiling((uint8_t)_addr, ceiling);
}

bool ESP32P4_Camera::getHMirror(bool *out) const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_hmirror || _addr <= 0) return false;
  return ops->get_hmirror((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getVFlip(bool *out) const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_vflip || _addr <= 0) return false;
  return ops->get_vflip((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getAEC(bool *out) const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_aec || _addr <= 0) return false;
  return ops->get_aec((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getAGC(bool *out) const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_agc || _addr <= 0) return false;
  return ops->get_agc((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getExposure(uint16_t *lines) const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_exposure || _addr <= 0) return false;
  return ops->get_exposure((uint8_t)_addr, lines);
}

bool ESP32P4_Camera::getGain(uint16_t *gain) const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_gain || _addr <= 0) return false;
  return ops->get_gain((uint8_t)_addr, gain);
}

bool ESP32P4_Camera::getGainCeiling(uint16_t *ceiling) const {
  auto *ops = (const esp32p4_cam_sensor_ops_t *)_ops;
  if (!ops || !ops->get_gainceiling || _addr <= 0) return false;
  return ops->get_gainceiling((uint8_t)_addr, ceiling);
}

