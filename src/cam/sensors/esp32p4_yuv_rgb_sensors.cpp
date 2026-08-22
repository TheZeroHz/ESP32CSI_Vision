/*
 * Processed MIPI sensors (sensor ISP → RGB565/YUV). Tables Apache-2.0 Espressif.
 */
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include <Arduino.h>

static void fill_processed(esp32p4_cam_mode_t *mode_out, const char *name, uint16_t w, uint16_t h,
                           uint8_t lanes, int mbps, esp32p4_cam_in_fmt_t fmt,
                           esp32p4_cam_framesize_t tag, uint8_t fps, const void *regs, size_t n) {
  if (!mode_out) return;
  mode_out->name = name;
  mode_out->width = w;
  mode_out->height = h;
  mode_out->lanes = lanes;
  mode_out->lane_mbps = mbps;
  mode_out->in_fmt = fmt;
  mode_out->bayer = ESP32P4_BAYER_NONE;
  mode_out->framesize_tag = tag;
  mode_out->regs = (const esp32p4_reg8_t *)regs;
  mode_out->regs_count = n;
  mode_out->fps = fps;
}

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

/* ---- OV5645 RGB565 / YUV422 (5MP bypass) ---- */
namespace ov5645_drv {
#include "cam/sensors/ov5645/ov5645_types.h"
#include "cam/sensors/ov5645/ov5645_regs.h"
#ifndef CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE
#define CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE 0
#endif
#include "cam/sensors/ov5645/ov5645_settings.h"

static const uint8_t kAddrs[] = {0x3C, 0x3D, 0};
static bool detect(uint8_t *addr7_out) { return detect_pid16(kAddrs, 0x300A, 0x5645, addr7_out); }
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x3008, 0x42); }
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x3008, 0x02); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)ov5645_mipi_2lane_24Minput_1280x960_rgb565_le_30fps;
  const char *name = "OV5645 1280x960 RGB565";
  uint16_t w = 1280, h = 960;
  int mbps = 448;
  uint8_t fps = 30;
  esp32p4_cam_in_fmt_t fmt = ESP32P4_CAM_IN_RGB565;
  esp32p4_cam_framesize_t tag = ESP32P4_FRAMESIZE_SXGA;
  if (want == ESP32P4_FRAMESIZE_VGA) {
    regs = (const esp32p4_reg8_t *)ov5645_mipi_2lane_24Minput_640x480_yuv422_uyvy_24fps;
    name = "OV5645 640x480 YUV422";
    w = 640;
    h = 480;
    mbps = 416;
    fps = 24;
    fmt = ESP32P4_CAM_IN_YUV422;
    tag = ESP32P4_FRAMESIZE_VGA;
  } else if (want == ESP32P4_FRAMESIZE_1080P) {
    regs = (const esp32p4_reg8_t *)ov5645_mipi_2lane_24Minput_1920x1080_yuv422_uyvy_15fps;
    name = "OV5645 1920x1080 YUV422";
    w = 1920;
    h = 1080;
    mbps = 336;
    fps = 15;
    fmt = ESP32P4_CAM_IN_YUV422;
    tag = ESP32P4_FRAMESIZE_1080P;
  } else if (want == ESP32P4_FRAMESIZE_5MP) {
    regs = (const esp32p4_reg8_t *)ov5645_mipi_2lane_24Minput_2592x1944_yuv422_uyvy_15fps;
    name = "OV5645 2592x1944 YUV422";
    w = 2592;
    h = 1944;
    mbps = 672;
    fps = 15;
    fmt = ESP32P4_CAM_IN_YUV422;
    tag = ESP32P4_FRAMESIZE_5MP;
  }
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  fill_processed(mode_out, name, w, h, 2, mbps, fmt, tag, fps, regs, n);
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_OV5645, "OV5645", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on,
    stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *ov5645_sensor_ops(void) { return &ov5645_drv::kOps; }

/* ---- OV5640 MIPI RGB565 HD (Espressif; DVP skipped) ---- */
namespace ov5640_drv {
#include "cam/sensors/ov5640/ov5640_types.h"
#include "cam/sensors/ov5640/ov5640_regs.h"
#include "cam/sensors/ov5640/ov5640_settings.h"
static const uint8_t kAddrs[] = {0x3C, 0x3D, 0};
static bool detect(uint8_t *out) { return detect_pid16(kAddrs, 0x300A, 0x5640, out); }
static bool stream_off(uint8_t a) { return esp32p4_sccb_write8(a, 0x3008, OV5640_SOFT_POWER_DOWN_EN); }
static bool stream_on(uint8_t a) { return esp32p4_sccb_write8(a, 0x3008, OV5640_SOFT_POWER_DOWN_DIS); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs =
      (const esp32p4_reg8_t *)ov5640_mipi_2lane_24Minput_1280x720_rgb565_le_14fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  fill_processed(mode_out, "OV5640 1280x720 RGB565", 1280, 720, 2, 320, ESP32P4_CAM_IN_RGB565,
                 ESP32P4_FRAMESIZE_HD, 14, regs, n);
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_OV5640, "OV5640", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on,
    stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *ov5640_sensor_ops(void) { return &ov5640_drv::kOps; }

/* ---- GC2145 RGB565 (sensor ISP; skip P4 IPA). 0xf9 is CM_MODE, not a delay. ---- */
namespace gc2145_drv {
#include "cam/sensors/gc2145/gc2145_types.h"
#include "cam/sensors/gc2145/gc2145_regs.h"
#include "cam/sensors/gc2145/gc2145_mipi_1lane_24Minput_640x480_rgb565_le_15fps.h"
#include "cam/sensors/gc2145/gc2145_mipi_1lane_24Minput_800x600_rgb565_le_30fps.h"
#include "cam/sensors/gc2145/gc2145_mipi_1lane_24Minput_1600x1200_rgb565_le_7fps.h"
static const uint8_t kAddrs[] = {0x3C, 0};
static bool write_table(uint8_t addr7, const gc2145_reginfo_t *regs, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (regs[i].reg == GC2145_REG_DELAY) {
      delay(regs[i].val ? regs[i].val : 1);
      continue;
    }
    if (!esp32p4_sccb_write8_reg8(addr7, regs[i].reg, regs[i].val)) return false;
  }
  return true;
}
static bool detect(uint8_t *a) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    if (!esp32p4_sccb_write8_reg8(*p, GC2145_REG_RESET_RELATED, 0x00)) continue;
    uint8_t hi = 0, lo = 0;
    if (!esp32p4_sccb_read8_reg8(*p, GC2145_REG_CHIP_ID_HIGH, &hi)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, GC2145_REG_CHIP_ID_LOW, &lo)) continue;
    if (((uint16_t)((hi << 8) | lo)) == 0x2145) {
      if (a) *a = *p;
      return true;
    }
  }
  return false;
}
static bool stream_off(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, GC2145_REG_RESET_RELATED, 0x00) &&
         esp32p4_sccb_write8_reg8(addr7, 0xfe, 0x03) &&
         esp32p4_sccb_write8_reg8(addr7, 0x10, 0x84);
}
static bool stream_on(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, GC2145_REG_RESET_RELATED, 0x00) &&
         esp32p4_sccb_write8_reg8(addr7, 0xfe, 0x03) &&
         esp32p4_sccb_write8_reg8(addr7, 0x10, 0x94);
}
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  stream_off(addr7);
  delay(5);
  const gc2145_reginfo_t *regs = gc2145_mipi_1lane_24Minput_800x600_rgb565_le_30fps;
  size_t n = sizeof(gc2145_mipi_1lane_24Minput_800x600_rgb565_le_30fps) /
             sizeof(gc2145_mipi_1lane_24Minput_800x600_rgb565_le_30fps[0]);
  uint16_t w = 800, h = 600;
  const char *name = "GC2145 800x600 RGB565";
  esp32p4_cam_framesize_t tag = ESP32P4_FRAMESIZE_SVGA;
  uint8_t fps = 30;
  if (want == ESP32P4_FRAMESIZE_VGA) {
    regs = gc2145_mipi_1lane_24Minput_640x480_rgb565_le_15fps;
    n = sizeof(gc2145_mipi_1lane_24Minput_640x480_rgb565_le_15fps) /
        sizeof(gc2145_mipi_1lane_24Minput_640x480_rgb565_le_15fps[0]);
    w = 640;
    h = 480;
    name = "GC2145 640x480 RGB565";
    tag = ESP32P4_FRAMESIZE_VGA;
    fps = 15;
  } else if (want == ESP32P4_FRAMESIZE_QXGA || want == ESP32P4_FRAMESIZE_5MP) {
    regs = gc2145_mipi_1lane_24Minput_1600x1200_rgb565_le_7fps;
    n = sizeof(gc2145_mipi_1lane_24Minput_1600x1200_rgb565_le_7fps) /
        sizeof(gc2145_mipi_1lane_24Minput_1600x1200_rgb565_le_7fps[0]);
    w = 1600;
    h = 1200;
    name = "GC2145 1600x1200 RGB565";
    tag = ESP32P4_FRAMESIZE_QXGA;
    fps = 7;
  }
  if (!write_table(addr7, regs, n)) return false;
  fill_processed(mode_out, name, w, h, 1, 336, ESP32P4_CAM_IN_RGB565, tag, fps, nullptr, n);
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_GC2145, "GC2145", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on,
    stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *gc2145_sensor_ops(void) { return &gc2145_drv::kOps; }

/* ---- LT6911 HDMI→CSI (YUYV). Burn Espressif LT6911D firmware on the bridge first. ---- */
namespace lt6911_drv {
#include "cam/sensors/lt6911/lt6911_types.h"
#include "cam/sensors/lt6911/lt6911_regs.h"
#include "cam/sensors/lt6911/lt6911_mipi_2lane_24Minput_1280x720_yuv422_yuyv_60fps.h"
#include "cam/sensors/lt6911/lt6911_mipi_2lane_24Minput_1920x1080_yuv422_yuyv_48fps.h"
static const uint8_t kAddrs[] = {0x2B, 0};
static bool write_a8(uint8_t addr7, const lt6911_reginfo_t *regs, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (regs[i].reg == LT6911_REG_DELAY) {
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
    if (!esp32p4_sccb_write8_reg8(*p, 0xFF, 0xE0)) continue;
    if (!esp32p4_sccb_write8_reg8(*p, 0xEE, 0x01)) continue;
    if (!esp32p4_sccb_write8_reg8(*p, 0xFF, 0xE1)) continue;
    uint8_t hi = 0, lo = 0;
    if (!esp32p4_sccb_read8_reg8(*p, LT6911_REG_CHIP_ID_H, &hi)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, LT6911_REG_CHIP_ID_L, &lo)) continue;
    (void)esp32p4_sccb_write8_reg8(*p, 0xFF, 0xE0);
    (void)esp32p4_sccb_write8_reg8(*p, 0xEE, 0x00);
    if (((uint16_t)((hi << 8) | lo)) == 0x2102) {
      if (out) *out = *p;
      return true;
    }
  }
  return false;
}
static bool stream_off(uint8_t) { return true; }
static bool stream_on(uint8_t) { return true; }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  const bool hd = (want == ESP32P4_FRAMESIZE_HD);
  const lt6911_reginfo_t *regs = hd ? lt6911_mipi_2lane_24Minput_1280x720_yuv422_60fps
                                    : lt6911_mipi_2lane_24Minput_1920x1080_yuv422_48fps;
  size_t n = hd ? sizeof(lt6911_mipi_2lane_24Minput_1280x720_yuv422_60fps) /
                      sizeof(lt6911_mipi_2lane_24Minput_1280x720_yuv422_60fps[0])
                : sizeof(lt6911_mipi_2lane_24Minput_1920x1080_yuv422_48fps) /
                      sizeof(lt6911_mipi_2lane_24Minput_1920x1080_yuv422_48fps[0]);
  if (!write_a8(addr7, regs, n)) return false;
  if (hd) {
    fill_processed(mode_out, "LT6911 HDMI 1280x720 YUV422", 1280, 720, 2, 357, ESP32P4_CAM_IN_YUV422,
                   ESP32P4_FRAMESIZE_HD, 60, nullptr, n);
  } else {
    fill_processed(mode_out, "LT6911 HDMI 1920x1080 YUV422", 1920, 1080, 2, 654,
                   ESP32P4_CAM_IN_YUV422, ESP32P4_FRAMESIZE_1080P, 48, nullptr, n);
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_LT6911, "LT6911", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on,
    stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *lt6911_sensor_ops(void) { return &lt6911_drv::kOps; }

namespace sc121at_drv {
/* Espressif MIPI YUV table is REG_END only — cannot stream until ALG-TECH publishes regs. */
static const uint8_t kAddrs[] = {0x30, 0};
static bool detect(uint8_t *a) { return detect_pid16(kAddrs, 0x3107, 0x2ada, a); }
static bool configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stream_on(uint8_t) { return false; }
static bool stream_off(uint8_t) { return true; }
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_SC121AT, "SC121AT", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, configure,
    stream_on, stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace
const esp32p4_cam_sensor_ops_t *sc121at_sensor_ops(void) { return &sc121at_drv::kOps; }
