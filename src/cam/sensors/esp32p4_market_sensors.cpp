/*
 * Market / Pi-class CSI sensors beyond Espressif esp_cam_sensor list.
 */
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include <Arduino.h>

namespace imx219_drv {
static const uint8_t kAddrs[] = {0x10, 0};
static bool detect(uint8_t *out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t id = 0;
    if (!esp32p4_sccb_read16(*p, 0x0000, &id)) continue;
    if (id == 0x0219) {
      if (out) *out = *p;
      return true;
    }
  }
  return false;
}
static const esp32p4_reg8_t kMode1080[] = {
    {0x0100, 0x00}, {0x30eb, 0x05}, {0x30eb, 0x0c}, {0x300a, 0xff}, {0x300b, 0xff},
    {0x30eb, 0x05}, {0x30eb, 0x09}, {0x0114, 0x01}, {0x0128, 0x00}, {0x012a, 0x18},
    {0x012b, 0x00}, {0x0160, 0x04}, {0x0161, 0x59}, {0x0162, 0x0d}, {0x0163, 0x78},
    {0x0164, 0x00}, {0x0165, 0x00}, {0x0166, 0x0c}, {0x0167, 0xcf}, {0x0168, 0x00},
    {0x0169, 0x00}, {0x016a, 0x09}, {0x016b, 0x9f}, {0x016c, 0x07}, {0x016d, 0x80},
    {0x016e, 0x04}, {0x016f, 0x38}, {0x0170, 0x01}, {0x0171, 0x01}, {0x0174, 0x00},
    {0x0175, 0x00}, {0x018c, 0x0a}, {0x018d, 0x0a}, {0x0301, 0x05}, {0x0303, 0x01},
    {0x0304, 0x03}, {0x0305, 0x03}, {0x0306, 0x00}, {0x0307, 0x39}, {0x0309, 0x0a},
    {0x030b, 0x01}, {0x030c, 0x00}, {0x030d, 0x72}, {0x0624, 0x07}, {0x0625, 0x80},
    {0x0626, 0x04}, {0x0627, 0x38}, {0xFFFF, 0x00},
};
static bool stream_off(uint8_t a) { return esp32p4_sccb_write8(a, 0x0100, 0x00); }
static bool stream_on(uint8_t a) { return esp32p4_sccb_write8(a, 0x0100, 0x01); }
static bool wr16(uint8_t a, uint16_t reg, uint16_t v) {
  return esp32p4_sccb_write8(a, reg, (uint8_t)(v >> 8)) &&
         esp32p4_sccb_write8(a, (uint16_t)(reg + 1), (uint8_t)v);
}
static bool rd16(uint8_t a, uint16_t reg, uint16_t *v) {
  uint8_t hi = 0, lo = 0;
  if (!esp32p4_sccb_read8(a, reg, &hi) || !esp32p4_sccb_read8(a, (uint16_t)(reg + 1), &lo)) return false;
  if (v) *v = (uint16_t)((hi << 8) | lo);
  return true;
}
static bool set_exposure(uint8_t a, uint16_t lines) {
  if (lines < 1) lines = 1;
  return wr16(a, 0x015A, lines); /* COARSE_INTEGRATION_TIME */
}
static bool get_exposure(uint8_t a, uint16_t *lines) { return rd16(a, 0x015A, lines); }
static bool set_gain(uint8_t a, uint16_t gain) {
  if (gain > 232) gain = 232; /* analogue gain 1.0–10.66 */
  return esp32p4_sccb_write8(a, 0x0157, (uint8_t)gain);
}
static bool get_gain(uint8_t a, uint16_t *gain) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, 0x0157, &v)) return false;
  if (gain) *gain = v;
  return true;
}
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  if (!esp32p4_cam_write_reg8_table(addr7, kMode1080, 0)) return false;
  if (mode_out) {
    mode_out->name = "IMX219 1920x1080 RAW10";
    mode_out->width = 1920;
    mode_out->height = 1080;
    mode_out->lanes = 2;
    mode_out->lane_mbps = 456;
    mode_out->in_fmt = ESP32P4_CAM_IN_RAW10;
    mode_out->bayer = ESP32P4_BAYER_RGGB;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_1080P;
    mode_out->regs = kMode1080;
    mode_out->regs_count = esp32p4_cam_reg8_count(kMode1080);
    mode_out->fps = 30;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX219, "IMX219", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure,
    stream_on, stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    set_exposure, get_exposure, set_gain, get_gain, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *imx219_sensor_ops(void) { return &imx219_drv::kOps; }

static bool ss_detect(const uint8_t *addrs, uint16_t pid, uint8_t *out) {
  for (const uint8_t *p = addrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x3107, &got)) continue;
    if (got == pid) {
      if (out) *out = *p;
      return true;
    }
  }
  return false;
}

namespace gc2083_drv {
static const uint8_t kAddrs[] = {0x37, 0x10, 0};
static bool detect(uint8_t *o) { return ss_detect(kAddrs, 0x2083, o); }
static bool configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stream_on(uint8_t) { return false; }
static bool stream_off(uint8_t) { return true; }
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_GC2083, "GC2083", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, configure,
    stream_on, stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *gc2083_sensor_ops(void) { return &gc2083_drv::kOps; }

namespace gc2093_drv {
static const uint8_t kAddrs[] = {0x37, 0x10, 0};
static bool detect(uint8_t *o) { return ss_detect(kAddrs, 0x2093, o); }
static bool configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stream_on(uint8_t) { return false; }
static bool stream_off(uint8_t) { return true; }
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_GC2093, "GC2093", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, configure,
    stream_on, stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *gc2093_sensor_ops(void) { return &gc2093_drv::kOps; }

namespace imx335_drv {
static const uint8_t kAddrs[] = {0x1A, 0x10, 0};
static bool detect(uint8_t *o) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t id = 0;
    if (esp32p4_sccb_read16(*p, 0x0016, &id) && (id == 0x0800 || id == 0x0335)) {
      if (o) *o = *p;
      return true;
    }
  }
  return false;
}
static bool configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stream_on(uint8_t) { return false; }
static bool stream_off(uint8_t) { return true; }
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX335, "IMX335", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, configure,
    stream_on, stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *imx335_sensor_ops(void) { return &imx335_drv::kOps; }

namespace imx415_drv {
static const uint8_t kAddrs[] = {0x1A, 0x10, 0};
static bool detect(uint8_t *o) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t id = 0;
    if (esp32p4_sccb_read16(*p, 0x3F12, &id) && id == 0x0415) {
      if (o) *o = *p;
      return true;
    }
  }
  return false;
}
static bool configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stream_on(uint8_t) { return false; }
static bool stream_off(uint8_t) { return true; }
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX415, "IMX415", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, configure,
    stream_on, stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *imx415_sensor_ops(void) { return &imx415_drv::kOps; }
