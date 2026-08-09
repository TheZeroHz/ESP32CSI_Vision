/*
 * Processed MIPI sensors (sensor ISP → RGB565/YUV). Tables Apache-2.0 Espressif.
 */
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include <Arduino.h>

/* ---- OV5645 RGB565 ---- */
namespace ov5645_drv {
#include "cam/sensors/ov5645/ov5645_types.h"
#include "cam/sensors/ov5645/ov5645_regs.h"
#ifndef CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE
#define CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE 0
#endif
#define OV5645_IDI_CLOCK_RATE_1280x960_30FPS (112000000ULL)
#define ov5645_settings_rgb565_le \
  {FORMAT_CTRL0, 0x6F}, {FORMAT_MUX_CTRL, 0x01}
#include "cam/sensors/ov5645/ov5645_mipi_2lane_24Minput_1280x960_rgb565_le_30fps.h"

static const uint8_t kAddrs[] = {0x3C, 0x3D, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t pid = 0;
    if (!esp32p4_sccb_read16(*p, 0x300A, &pid)) continue;
    if (pid == 0x5645) {
      if (addr7_out) *addr7_out = *p;
      return true;
    }
  }
  return false;
}
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x3008, 0x42); }
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x3008, 0x02); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs =
      (const esp32p4_reg8_t *)ov5645_mipi_2lane_24Minput_1280x960_rgb565_le_30fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "OV5645 1280x960 RGB565";
    mode_out->width = 1280;
    mode_out->height = 960;
    mode_out->lanes = 2;
    mode_out->lane_mbps = 448;
    mode_out->in_fmt = ESP32P4_CAM_IN_RGB565;
    mode_out->bayer = ESP32P4_BAYER_NONE;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_SXGA;
    mode_out->regs = regs;
    mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_OV5645,
    "OV5645",
    ESP32P4_CAM_SUPPORT_EXPERIMENTAL,
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
};
}  // namespace
const esp32p4_cam_sensor_ops_t *ov5645_sensor_ops(void) { return &ov5645_drv::kOps; }

/* ---- GC2145 / SC121AT: detect-only (8-bit SCCB / YUV path TBD) ---- */
static bool detect_pid16(const uint8_t *addrs, uint16_t id_reg, uint16_t pid, uint8_t *out) {
  for (const uint8_t *p = addrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, id_reg, &got)) continue;
    if (got == pid) {
      if (out) *out = *p;
      return true;
    }
  }
  return false;
}

namespace gc2145_drv {
static const uint8_t kAddrs[] = {0x3C, 0};
static bool detect(uint8_t *a) {
  // GC2145 chip id at 0xf0/0xf1 via 8-bit SCCB
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint8_t hi = 0, lo = 0;
    if (!esp32p4_sccb_read8_reg8(*p, 0xf0, &hi)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, 0xf1, &lo)) continue;
    if (((hi << 8) | lo) == 0x2145) {
      if (a) *a = *p;
      return true;
    }
  }
  return false;
}
static bool configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stream_on(uint8_t) { return false; }
static bool stream_off(uint8_t) { return true; }
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_GC2145,
    "GC2145",
    ESP32P4_CAM_SUPPORT_DETECT_ONLY,
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
};
}  // namespace
const esp32p4_cam_sensor_ops_t *gc2145_sensor_ops(void) { return &gc2145_drv::kOps; }

namespace sc121at_drv {
static const uint8_t kAddrs[] = {0x30, 0};
static bool detect(uint8_t *a) { return detect_pid16(kAddrs, 0x3107, 0x2ada, a); }
static bool configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stream_on(uint8_t) { return false; }
static bool stream_off(uint8_t) { return true; }
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_SC121AT,
    "SC121AT",
    ESP32P4_CAM_SUPPORT_DETECT_ONLY,
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
};
}  // namespace
const esp32p4_cam_sensor_ops_t *sc121at_sensor_ops(void) { return &sc121at_drv::kOps; }
