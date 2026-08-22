#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include "cam/sensors/imx477/imx477_settings.h"

#include <Arduino.h>

namespace imx477_drv {
static const uint8_t kAddrs[] = {0x1A, 0x10, 0};

static bool wr16(uint8_t a, uint16_t reg, uint16_t v) {
  return esp32p4_sccb_write8(a, reg, (uint8_t)(v >> 8)) &&
         esp32p4_sccb_write8(a, (uint16_t)(reg + 1), (uint8_t)v);
}

static bool rd16(uint8_t a, uint16_t reg, uint16_t *v) {
  uint8_t hi = 0, lo = 0;
  if (!esp32p4_sccb_read8(a, reg, &hi) || !esp32p4_sccb_read8(a, (uint16_t)(reg + 1), &lo)) {
    return false;
  }
  if (v) *v = (uint16_t)((hi << 8) | lo);
  return true;
}

static bool detect(uint8_t *out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t id = 0;
    if (!esp32p4_sccb_read16(*p, IMX477_REG_CHIP_ID, &id)) continue;
    if (id == IMX477_CHIP_ID || id == IMX378_CHIP_ID) {
      if (out) *out = *p;
      return true;
    }
  }
  return false;
}

static bool stream_off(uint8_t a) {
  return esp32p4_sccb_write8(a, IMX477_REG_MODE_SELECT, IMX477_MODE_STANDBY);
}

static bool stream_on(uint8_t a) {
  return esp32p4_sccb_write8(a, IMX477_REG_MODE_SELECT, IMX477_MODE_STREAMING);
}

static bool set_hmirror(uint8_t a, bool en) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX477_REG_ORIENTATION, &v)) return false;
  if (en) v |= 0x01;
  else v &= (uint8_t)~0x01;
  return esp32p4_sccb_write8(a, IMX477_REG_ORIENTATION, v);
}

static bool set_vflip(uint8_t a, bool en) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX477_REG_ORIENTATION, &v)) return false;
  if (en) v |= 0x02;
  else v &= (uint8_t)~0x02;
  return esp32p4_sccb_write8(a, IMX477_REG_ORIENTATION, v);
}

static bool get_hmirror(uint8_t a, bool *out) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX477_REG_ORIENTATION, &v)) return false;
  if (out) *out = (v & 0x01) != 0;
  return true;
}

static bool get_vflip(uint8_t a, bool *out) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX477_REG_ORIENTATION, &v)) return false;
  if (out) *out = (v & 0x02) != 0;
  return true;
}

static bool set_exposure(uint8_t a, uint16_t lines) {
  if (lines < 1) lines = 1;
  return wr16(a, IMX477_REG_EXPOSURE, lines);
}

static bool get_exposure(uint8_t a, uint16_t *lines) { return rd16(a, IMX477_REG_EXPOSURE, lines); }

static bool set_gain(uint8_t a, uint16_t gain) {
  if (gain < 0x100) gain = 0x100; /* 1.0× */
  if (gain > 0xF00) gain = 0xF00;
  return wr16(a, IMX477_REG_ANA_GAIN, gain);
}

static bool get_gain(uint8_t a, uint16_t *gain) { return rd16(a, IMX477_REG_ANA_GAIN, gain); }

static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  stream_off(addr7);
  delay(5);
  if (!esp32p4_cam_write_reg8_table(addr7, imx477_common, 0)) return false;
  delay(10);

  esp32p4_cam_mode_t m{};
  m.bayer = ESP32P4_BAYER_RGGB;
  m.in_fmt = ESP32P4_CAM_IN_RAW10;
  m.lanes = 2;
  m.fps = 30;
  const bool hd = (want == ESP32P4_FRAMESIZE_HD || want == ESP32P4_FRAMESIZE_VGA ||
                   want == ESP32P4_FRAMESIZE_SVGA || want == ESP32P4_FRAMESIZE_800X640);
  if (hd) {
    if (!esp32p4_cam_write_reg8_table(addr7, imx477_1332x990_raw10, 0)) return false;
    m.name = "IMX477 1332x990 RAW10";
    m.width = 1332;
    m.height = 990;
    m.lane_mbps = 450;
    m.framesize_tag = ESP32P4_FRAMESIZE_HD;
    m.regs = imx477_1332x990_raw10;
  } else {
    if (!esp32p4_cam_write_reg8_table(addr7, imx477_1920x1080_raw10, 0)) return false;
    if (!esp32p4_cam_write_reg8_table(addr7, imx477_link_450mhz, 0)) return false;
    m.name = "IMX477 1920x1080 RAW10";
    m.width = 1920;
    m.height = 1080;
    m.lane_mbps = 450;
    m.framesize_tag = ESP32P4_FRAMESIZE_1080P;
    m.regs = imx477_1920x1080_raw10;
  }
  m.regs_count = esp32p4_cam_reg8_count(m.regs);
  if (mode_out) *mode_out = m;
  return true;
}

static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX477,
    "IMX477",
    ESP32P4_CAM_SUPPORT_FULL,
    kAddrs,
    detect,
    configure,
    stream_on,
    stream_off,
    set_hmirror,
    set_vflip,
    get_hmirror,
    get_vflip,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    set_exposure,
    get_exposure,
    set_gain,
    get_gain,
    nullptr,
    nullptr,
    nullptr};
}  // namespace imx477_drv

const esp32p4_cam_sensor_ops_t *imx477_sensor_ops(void) { return &imx477_drv::kOps; }
