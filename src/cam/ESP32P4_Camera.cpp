#include "cam/ESP32P4_Camera.h"

#include <Wire.h>
#include <string.h>

#include "cam/sensors/imx708_sensor.h"
#include "cam/sensors/ov5647_sensor.h"
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
#include "mem/ESP32P4_Psram.h"

static SemaphoreHandle_t s_frame_sem = nullptr;
static esp_cam_ctlr_trans_t s_trans[ESP32P4_CAM_FB_MAX]{};
static volatile int s_write_idx = 0;
static volatile int s_done_idx = 0;
static volatile uint32_t s_new_trans_count = 0;
static volatile uint32_t s_done_count = 0;
static uint8_t s_fb_n = 1;

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

static bool IRAM_ATTR on_get_new_trans(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *trans, void *) {
  s_new_trans_count++;
  *trans = s_trans[s_write_idx];
  return false;
}

static bool IRAM_ATTR on_trans_finished(esp_cam_ctlr_handle_t, esp_cam_ctlr_trans_t *, void *) {
  s_done_count++;
  s_done_idx = s_write_idx;
  s_write_idx = (s_write_idx + 1) % (s_fb_n ? s_fb_n : 1);
  BaseType_t woken = pdFALSE;
  if (s_frame_sem) xSemaphoreGiveFromISR(s_frame_sem, &woken);
  return woken == pdTRUE;
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
  c.frame_size = ESP32P4_FRAMESIZE_800X640;
  c.pixel_format = ESP32P4_PIXFORMAT_RGB565;
  c.lane_bit_rate_mbps = 200;
  c.sensor = ESP32P4_SENSOR_OV5647;
  c.test_pattern = false;
  c.fb_count = 2;
  (void)board;
  return c;
}

const char *ESP32P4_Camera::sensorName() const {
  switch (_sensor) {
    case ESP32P4_SENSOR_OV5647: return "OV5647 (OV CSI)";
    case ESP32P4_SENSOR_IMX708: return "IMX708 (Pi Cam 3)";
    default: return "unknown";
  }
}

uint32_t ESP32P4_Camera::newTransCount() const { return s_new_trans_count; }
uint32_t ESP32P4_Camera::doneCount() const { return s_done_count; }
bool ESP32P4_Camera::psramOk() const { return esp32p4_psram_available(); }

bool ESP32P4_Camera::probe_sensor() {
  uint8_t a = 0;
  if (_cfg.sensor != ESP32P4_SENSOR_IMX708) {
    if (ov5647_detect(&a)) {
      _addr = a;
      _sensor = ESP32P4_SENSOR_OV5647;
      return true;
    }
  }
  if (_cfg.sensor != ESP32P4_SENSOR_OV5647) {
    if (imx708_detect(&a)) {
      _addr = a;
      _sensor = ESP32P4_SENSOR_IMX708;
      return true;
    }
  }
  return false;
}

bool ESP32P4_Camera::init_sensor() {
  _sensor = ESP32P4_SENSOR_AUTO;
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

  Serial.printf("CSI: I2C scan SDA=%d SCL=%d\n", _cfg.sda, _cfg.scl);
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
    Serial.println("CSI: no OV5647 (0x36) or IMX708 (0x1A) detected");
    return false;
  }
  Serial.printf("CSI: %s @ 0x%02X\n", sensorName(), _addr);

  bool ok = false;
  if (_sensor == ESP32P4_SENSOR_OV5647) {
    _w = 800;
    _h = 640;
    _raw8 = true;
    _cfg.pixel_format = ESP32P4_PIXFORMAT_RGB565;
    if (_cfg.lane_bit_rate_mbps <= 0) _cfg.lane_bit_rate_mbps = 200;
    ok = ov5647_configure_800x640_raw8((uint8_t)_addr);
  } else if (_cfg.frame_size == ESP32P4_FRAMESIZE_2304X1296) {
    _w = 2304;
    _h = 1296;
    _raw8 = false;
    ok = imx708_configure_2304x1296((uint8_t)_addr);
  } else {
    _w = 1280;
    _h = 720;
    _raw8 = false;
    ok = imx708_configure_hd720((uint8_t)_addr);
  }
  return ok;
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
  if (_fb_n < 1) _fb_n = 1;
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
  }
  s_fb_n = _fb_n;
  s_write_idx = 0;
  s_done_idx = 0;
  Serial.printf("CSI: %u PSRAM framebuffers x %u bytes (PSRAM free=%u)\n", _fb_n, (unsigned)_fb_cap,
                (unsigned)esp32p4_psram_free_size());
  return true;
}

bool ESP32P4_Camera::init_csi_isp() {
  const int bitrate = _cfg.lane_bit_rate_mbps > 0 ? _cfg.lane_bit_rate_mbps : 200;
  const bool rgb = (_cfg.pixel_format == ESP32P4_PIXFORMAT_RGB565);

  if (rgb) {
    esp_isp_processor_cfg_t isp_cfg = {};
    isp_cfg.clk_hz = 80 * 1000 * 1000;
    isp_cfg.input_data_source = ISP_INPUT_DATA_SOURCE_CSI;
    isp_cfg.input_data_color_type = _raw8 ? ISP_COLOR_RAW8 : ISP_COLOR_RAW10;
    isp_cfg.output_data_color_type = ISP_COLOR_RGB565;
    isp_cfg.has_line_start_packet = false;
    isp_cfg.has_line_end_packet = false;
    isp_cfg.h_res = _w;
    isp_cfg.v_res = _h;
    isp_cfg.bayer_order = (_sensor == ESP32P4_SENSOR_OV5647) ? COLOR_RAW_ELEMENT_ORDER_GBRG
                                                             : COLOR_RAW_ELEMENT_ORDER_RGGB;
    isp_proc_handle_t isp = nullptr;
    if (esp_isp_new_processor(&isp_cfg, &isp) != ESP_OK) return false;
    if (esp_isp_enable(isp) != ESP_OK) return false;
    esp_isp_demosaic_config_t demosaic = {};
    demosaic.padding_mode = ISP_DEMOSAIC_EDGE_PADDING_MODE_SRND_DATA;
    if (esp_isp_demosaic_configure(isp, &demosaic) != ESP_OK) return false;
    if (esp_isp_demosaic_enable(isp) != ESP_OK) return false;
    _isp = isp;
    Serial.println("CSI: ISP RAW->RGB565 + demosaic");
  }

  esp_cam_ctlr_csi_config_t csi_cfg = {};
  csi_cfg.ctlr_id = 0;
  csi_cfg.h_res = _w;
  csi_cfg.v_res = _h;
  csi_cfg.data_lane_num = 2;
  csi_cfg.lane_bit_rate_mbps = bitrate;
  csi_cfg.input_data_color_type = _raw8 ? CAM_CTLR_COLOR_RAW8 : CAM_CTLR_COLOR_RAW10;
  csi_cfg.output_data_color_type = _raw8 ? CAM_CTLR_COLOR_RAW8 : CAM_CTLR_COLOR_RAW10;
  csi_cfg.queue_items = 1;
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
  Serial.printf("CSI: controller started %ux%u @ %d Mbps/lane\n", _w, _h, bitrate);
  return true;
}

bool ESP32P4_Camera::start_sensor_stream() {
  if (_sensor == ESP32P4_SENSOR_OV5647) {
    if (!ov5647_stream_restart((uint8_t)_addr)) return false;
    if (_cfg.test_pattern) ov5647_set_test_pattern((uint8_t)_addr, true);
    delay(200);
    ov5647_dump_key_regs((uint8_t)_addr);
    delay(400);
    return true;
  }
  return imx708_stream_on((uint8_t)_addr);
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
  if (!alloc_fbs()) return false;
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
  if (xSemaphoreTake(s_frame_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return nullptr;
  int idx = s_done_idx;
  if (idx < 0 || idx >= _fb_n) return nullptr;
  esp32p4_psram_msync(_fb[idx].buf, _fb_cap);
  _fb[idx].len = _fb_cap;
  _fb[idx].timestamp_us = (uint32_t)esp_timer_get_time();
  return &_fb[idx];
}

void ESP32P4_Camera::release(camera_fb_t *) {}

bool ESP32P4_Camera::setTestPattern(bool enable) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  _cfg.test_pattern = enable;
  return ov5647_set_test_pattern((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setHMirror(bool enable) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_set_hmirror((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setVFlip(bool enable) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_set_vflip((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setAEC(bool enable) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_set_aec((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setAGC(bool enable) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_set_agc((uint8_t)_addr, enable);
}

bool ESP32P4_Camera::setExposure(uint16_t lines) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_set_exposure((uint8_t)_addr, lines);
}

bool ESP32P4_Camera::setGain(uint16_t gain) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_set_gain((uint8_t)_addr, gain);
}

bool ESP32P4_Camera::setGainCeiling(uint16_t ceiling) {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_set_gainceiling((uint8_t)_addr, ceiling);
}

bool ESP32P4_Camera::getHMirror(bool *out) const {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_get_hmirror((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getVFlip(bool *out) const {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_get_vflip((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getAEC(bool *out) const {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_get_aec((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getAGC(bool *out) const {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_get_agc((uint8_t)_addr, out);
}

bool ESP32P4_Camera::getExposure(uint16_t *lines) const {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_get_exposure((uint8_t)_addr, lines);
}

bool ESP32P4_Camera::getGain(uint16_t *gain) const {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_get_gain((uint8_t)_addr, gain);
}

bool ESP32P4_Camera::getGainCeiling(uint16_t *ceiling) const {
  if (_sensor != ESP32P4_SENSOR_OV5647 || _addr <= 0) return false;
  return ov5647_get_gainceiling((uint8_t)_addr, ceiling);
}

