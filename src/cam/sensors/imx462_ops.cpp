#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include "cam/sensors/imx462/imx462_settings.h"

#include <Arduino.h>

namespace imx462_drv {
static const uint8_t kAddrs[] = {0x1A, 0};

static bool is_pi_cmos_id(uint16_t id) {
  return id == 0x0477 || id == 0x0378 || id == 0x0708 || id == 0x0219;
}

static bool detect(uint8_t *out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t cmos = 0;
    if (esp32p4_sccb_read16(*p, 0x0016, &cmos) && is_pi_cmos_id(cmos)) continue;
    uint16_t sony4k = 0;
    if (esp32p4_sccb_read16(*p, 0x3F12, &sony4k) &&
        (sony4k == 0x0415 || sony4k == 0x0296 || sony4k == 0x296)) {
      continue;
    }
    uint8_t stby = 0xFF;
    if (!esp32p4_sccb_read8(*p, IMX462_STANDBY, &stby)) continue;
    uint8_t gain = 0xFF;
    if (!esp32p4_sccb_read8(*p, IMX462_GAIN, &gain)) continue;
    /* STARVIS family: standby is 0 or 1; gain is a small analogue code. */
    if (stby > 1) continue;
    if (out) *out = *p;
    return true;
  }
  return false;
}

static bool stream_off(uint8_t a) {
  (void)esp32p4_sccb_write8(a, IMX462_XMSTA, 0x01);
  return esp32p4_sccb_write8(a, IMX462_STANDBY, 0x01);
}

static bool stream_on(uint8_t a) {
  if (!esp32p4_sccb_write8(a, IMX462_STANDBY, 0x00)) return false;
  delay(30);
  return esp32p4_sccb_write8(a, IMX462_XMSTA, 0x00);
}

static bool set_hmirror(uint8_t a, bool en) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX462_FLIP_WINMODE, &v)) return false;
  if (en) v |= 0x02;
  else v &= (uint8_t)~0x02;
  return esp32p4_sccb_write8(a, IMX462_FLIP_WINMODE, v);
}

static bool set_vflip(uint8_t a, bool en) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX462_FLIP_WINMODE, &v)) return false;
  if (en) v |= 0x01;
  else v &= (uint8_t)~0x01;
  return esp32p4_sccb_write8(a, IMX462_FLIP_WINMODE, v);
}

static bool get_hmirror(uint8_t a, bool *out) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX462_FLIP_WINMODE, &v)) return false;
  if (out) *out = (v & 0x02) != 0;
  return true;
}

static bool get_vflip(uint8_t a, bool *out) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX462_FLIP_WINMODE, &v)) return false;
  if (out) *out = (v & 0x01) != 0;
  return true;
}

static uint32_t vmax(uint8_t a) {
  uint8_t l = 0, m = 0, h = 0;
  if (!esp32p4_sccb_read8(a, IMX462_VMAX_L, &l)) return 1125;
  if (!esp32p4_sccb_read8(a, (uint16_t)(IMX462_VMAX_L + 1), &m)) return 1125;
  if (!esp32p4_sccb_read8(a, (uint16_t)(IMX462_VMAX_L + 2), &h)) return 1125;
  uint32_t v = (uint32_t)l | ((uint32_t)m << 8) | ((uint32_t)(h & 0x0F) << 16);
  return v ? v : 1125;
}

static bool set_exposure(uint8_t a, uint16_t lines) {
  uint32_t vm = vmax(a);
  if (lines < 1) lines = 1;
  if (lines + 2 >= vm) lines = (uint16_t)(vm - 2);
  uint32_t shs1 = vm - (uint32_t)lines - 1;
  return esp32p4_sccb_write8(a, IMX462_SHS1_L, (uint8_t)(shs1 & 0xFF)) &&
         esp32p4_sccb_write8(a, (uint16_t)(IMX462_SHS1_L + 1), (uint8_t)((shs1 >> 8) & 0xFF)) &&
         esp32p4_sccb_write8(a, (uint16_t)(IMX462_SHS1_L + 2), (uint8_t)((shs1 >> 16) & 0x0F));
}

static bool get_exposure(uint8_t a, uint16_t *lines) {
  uint8_t l = 0, m = 0, h = 0;
  if (!esp32p4_sccb_read8(a, IMX462_SHS1_L, &l)) return false;
  if (!esp32p4_sccb_read8(a, (uint16_t)(IMX462_SHS1_L + 1), &m)) return false;
  if (!esp32p4_sccb_read8(a, (uint16_t)(IMX462_SHS1_L + 2), &h)) return false;
  uint32_t shs1 = (uint32_t)l | ((uint32_t)m << 8) | ((uint32_t)(h & 0x0F) << 16);
  uint32_t vm = vmax(a);
  uint32_t exp = (vm > shs1 + 1) ? (vm - shs1 - 1) : 1;
  if (lines) *lines = (uint16_t)exp;
  return true;
}

static bool set_gain(uint8_t a, uint16_t gain) {
  if (gain > 98) gain = 98;
  return esp32p4_sccb_write8(a, IMX462_GAIN, (uint8_t)gain);
}

static bool get_gain(uint8_t a, uint16_t *gain) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(a, IMX462_GAIN, &v)) return false;
  if (gain) *gain = v;
  return true;
}

static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  stream_off(addr7);
  delay(5);
  if (!esp32p4_cam_write_reg8_table(addr7, imx462_global, 0)) return false;
  if (!esp32p4_cam_write_reg8_table(addr7, imx462_raw10, 0)) return false;

  esp32p4_cam_mode_t m{};
  m.bayer = ESP32P4_BAYER_RGGB;
  m.in_fmt = ESP32P4_CAM_IN_RAW10;
  m.lanes = 2;
  m.fps = 30;
  const bool hd = (want == ESP32P4_FRAMESIZE_HD || want == ESP32P4_FRAMESIZE_VGA ||
                   want == ESP32P4_FRAMESIZE_SVGA || want == ESP32P4_FRAMESIZE_800X640);
  if (hd) {
    if (!esp32p4_cam_write_reg8_table(addr7, imx462_clk_37_125_720p, 0)) return false;
    if (!esp32p4_cam_write_reg8_table(addr7, imx462_720p_2lane, 0)) return false;
    m.name = "IMX462 1280x720 RAW10";
    m.width = 1280;
    m.height = 720;
    m.lane_mbps = 297;
    m.framesize_tag = ESP32P4_FRAMESIZE_HD;
    m.regs = imx462_720p_2lane;
  } else {
    if (!esp32p4_cam_write_reg8_table(addr7, imx462_clk_37_125_1080p, 0)) return false;
    if (!esp32p4_cam_write_reg8_table(addr7, imx462_1080p_2lane, 0)) return false;
    m.name = "IMX462 1920x1080 RAW10";
    m.width = 1920;
    m.height = 1080;
    m.lane_mbps = 446;
    m.framesize_tag = ESP32P4_FRAMESIZE_1080P;
    m.regs = imx462_1080p_2lane;
  }
  m.regs_count = esp32p4_cam_reg8_count(m.regs);
  if (mode_out) *mode_out = m;
  return true;
}

static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX462,
    "IMX462",
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
}  // namespace imx462_drv

const esp32p4_cam_sensor_ops_t *imx462_sensor_ops(void) { return &imx462_drv::kOps; }
