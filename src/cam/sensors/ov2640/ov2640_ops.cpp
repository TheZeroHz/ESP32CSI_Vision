/*
 * OV2640 DVP VGA RGB565 (Espressif Apache-2.0 tables). Not MIPI CSI.
 */
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include <Arduino.h>

namespace ov2640_drv {
#include "cam/sensors/ov2640/ov2640_types.h"
#include "cam/sensors/ov2640/ov2640_regs.h"
#include "cam/sensors/ov2640/ov2640_settings.h"

static const uint8_t kAddrs[] = {0x30, 0};

static const ov2640_reginfo_t kVgaRgb565Le[] = {
    init_reglist_DVP_8bit_640x480_XCLK_20_6fps_common,
    ov2640_settings_rgb565_le,
};

static bool write_table(uint8_t addr7, const ov2640_reginfo_t *regs, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (regs[i].reg == REG_DELAY) {
      delay(regs[i].val ? regs[i].val : 1);
      continue;
    }
    if (!esp32p4_sccb_write8_reg8(addr7, regs[i].reg, regs[i].val)) return false;
  }
  return true;
}

static bool detect(uint8_t *out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    if (!esp32p4_sccb_write8_reg8(*p, BANK_SEL, BANK_SENSOR)) continue;
    uint8_t pid = 0, ver = 0;
    if (!esp32p4_sccb_read8_reg8(*p, REG_PID, &pid)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, REG_VER, &ver)) continue;
    if (pid == 0x26 && (ver == 0x42 || ver == 0x41 || ver == 0x43)) {
      if (out) *out = *p;
      return true;
    }
  }
  return false;
}

static bool stream_off(uint8_t addr7) {
  if (!esp32p4_sccb_write8_reg8(addr7, BANK_SEL, BANK_SENSOR)) return false;
  uint8_t v = 0;
  if (!esp32p4_sccb_read8_reg8(addr7, COM2, &v)) return false;
  return esp32p4_sccb_write8_reg8(addr7, COM2, (uint8_t)(v | COM2_STDBY));
}

static bool stream_on(uint8_t addr7) {
  if (!esp32p4_sccb_write8_reg8(addr7, BANK_SEL, BANK_SENSOR)) return false;
  uint8_t v = 0;
  if (!esp32p4_sccb_read8_reg8(addr7, COM2, &v)) return false;
  return esp32p4_sccb_write8_reg8(addr7, COM2, (uint8_t)(v & (uint8_t)~COM2_STDBY));
}

static bool set_test_pattern(uint8_t addr7, bool enable) {
  if (!esp32p4_sccb_write8_reg8(addr7, BANK_SEL, BANK_SENSOR)) return false;
  uint8_t v = 0;
  if (!esp32p4_sccb_read8_reg8(addr7, COM7, &v)) return false;
  if (enable) v = (uint8_t)(v | COM7_COLOR_BAR);
  else v = (uint8_t)(v & (uint8_t)~COM7_COLOR_BAR);
  return esp32p4_sccb_write8_reg8(addr7, COM7, v);
}

static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  if (!write_table(addr7, ov2640_settings_cif,
                   sizeof(ov2640_settings_cif) / sizeof(ov2640_settings_cif[0]))) {
    return false;
  }
  if (!write_table(addr7, kVgaRgb565Le, sizeof(kVgaRgb565Le) / sizeof(kVgaRgb565Le[0]))) {
    return false;
  }
  if (!mode_out) return true;
  mode_out->name = "OV2640 DVP 640x480 RGB565";
  mode_out->width = 640;
  mode_out->height = 480;
  mode_out->lanes = 0;
  mode_out->lane_mbps = 0;
  mode_out->in_fmt = ESP32P4_CAM_IN_RGB565;
  mode_out->bayer = ESP32P4_BAYER_NONE;
  mode_out->framesize_tag = ESP32P4_FRAMESIZE_VGA;
  mode_out->regs = nullptr;
  mode_out->regs_count = 0;
  mode_out->fps = 6;
  mode_out->spi_frame_info = nullptr;
  return true;
}

static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_OV2640,
    "OV2640",
    ESP32P4_CAM_SUPPORT_FULL,
    kAddrs,
    detect,
    configure,
    stream_on,
    stream_off,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    ESP32P4_CAM_BUS_DVP,
};
}  // namespace ov2640_drv

const esp32p4_cam_sensor_ops_t *ov2640_sensor_ops(void) { return &ov2640_drv::kOps; }
