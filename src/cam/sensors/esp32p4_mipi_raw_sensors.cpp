/*
 * Auto-generated Arduino MIPI RAW sensor wrappers.
 * Register tables: SPDX-License-Identifier: Apache-2.0 (Espressif Systems).
 */
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include <Arduino.h>


/* ---- OV9281 pid=0x9281 ---- */
namespace ov9281_drv {
#include "cam/sensors/ov9281/ov9281_types.h"
#include "cam/sensors/ov9281/ov9281_regs.h"
#include "cam/sensors/ov9281/ov9281_mipi_2lane_24Minput_1280x720_raw8_50fps.h"
static const uint8_t kAddrs[] = {0x60, 0x10, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x300A, &got)) continue;
    if (got == 0x9281) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x01); }
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x00); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)MIPI_2lane_24Minput_RAW8_1280x720_50fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "OV9281 1280x720";
    mode_out->width = 1280; mode_out->height = 720; mode_out->lanes = 2;
    mode_out->lane_mbps = 400; mode_out->in_fmt = ESP32P4_CAM_IN_RAW8; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_HD; mode_out->regs = regs; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_OV9281, "OV9281", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *ov9281_sensor_ops(void) { return &ov9281_drv::kOps; }


/* ---- OS02N10 pid=0x534E (8-bit SCCB + banked regs) ---- */
namespace os02n10_drv {
#include "cam/sensors/os02n10/os02n10_types.h"
#include "cam/sensors/os02n10/os02n10_regs.h"
#include "cam/sensors/os02n10/os02n10_mipi_2lane_24Minput_1920x1080_raw10_25fps.h"
static const uint8_t kAddrs[] = {0x3C, 0x3D, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    if (!esp32p4_sccb_write8_reg8(*p, OS02N10_REG_BANK_SEL, 0x00)) continue;
    uint8_t hh = 0, hl = 0, lh = 0, ll = 0;
    if (!esp32p4_sccb_read8_reg8(*p, OS02N10_REG_CHIP_ID_ADDR_HH, &hh)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, OS02N10_REG_CHIP_ID_ADDR_HL, &hl)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, OS02N10_REG_CHIP_ID_ADDR_LH, &lh)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, OS02N10_REG_CHIP_ID_ADDR_LL, &ll)) continue;
    uint16_t got = (uint16_t)((hh << 8) | hl);
    (void)lh;
    (void)ll;
    if (got == 0x534E) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, OS02N10_REG_BANK_SEL, 0x00) &&
         esp32p4_sccb_write8_reg8(addr7, 0xfb, 0x03);
}
static bool stream_off(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, OS02N10_REG_BANK_SEL, 0x00) &&
         esp32p4_sccb_write8_reg8(addr7, 0xfb, 0x00);
}
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8a8_t *regs =
      (const esp32p4_reg8a8_t *)os02n10_mipi_2lane_24Minput_1920x1080_raw10_25fps;
  size_t n = sizeof(os02n10_mipi_2lane_24Minput_1920x1080_raw10_25fps) /
             sizeof(os02n10_mipi_2lane_24Minput_1920x1080_raw10_25fps[0]);
  if (!esp32p4_cam_write_reg8a8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "OS02N10 1920x1080";
    mode_out->width = 1920; mode_out->height = 1080; mode_out->lanes = 2;
    mode_out->lane_mbps = 480; mode_out->in_fmt = ESP32P4_CAM_IN_RAW10; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_1080P; mode_out->regs = nullptr; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_OS02N10, "OS02N10", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *os02n10_sensor_ops(void) { return &os02n10_drv::kOps; }


/* ---- SC035HGS pid=0x0031 ---- */
namespace sc035hgs_drv {
#include "cam/sensors/sc035hgs/sc035hgs_types.h"
#include "cam/sensors/sc035hgs/sc035hgs_regs.h"
#include "cam/sensors/sc035hgs/sc035hgs_mipi_2lane_24Minput_640x480_raw8_linear_50fps.h"
static const uint8_t kAddrs[] = {0x30, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x3107, &got)) continue;
    if (got == 0x0031) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x01); }
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x00); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)sc035hgs_mipi_2lane_24Minput_640x480_raw8_linear_50fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "SC035HGS 640x480";
    mode_out->width = 640; mode_out->height = 480; mode_out->lanes = 2;
    mode_out->lane_mbps = 360; mode_out->in_fmt = ESP32P4_CAM_IN_RAW8; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_VGA; mode_out->regs = regs; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_SC035HGS, "SC035HGS", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *sc035hgs_sensor_ops(void) { return &sc035hgs_drv::kOps; }


/* ---- OV2710 pid=0x2710 ---- */
namespace ov2710_drv {
#include "cam/sensors/ov2710/ov2710_types.h"
#include "cam/sensors/ov2710/ov2710_regs.h"
#include "cam/sensors/ov2710/ov2710_mipi_1lane_24Minput_1920x1080_raw10_25fps.h"
static const uint8_t kAddrs[] = {0x36, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x300A, &got)) continue;
    if (got == 0x2710) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) {
  // Espressif ov2710_set_stream(1): un-gate frames, then leave software sleep.
  return esp32p4_sccb_write8(addr7, OV2710_REG_FRAME_CTRL01, 0x00) &&
         esp32p4_sccb_write8(addr7, OV2710_REG_FRAME_CTRL02, 0x00) &&
         esp32p4_sccb_write8(addr7, OV2710_REG_SW_SLEEP, OV2710_SW_SLEEP_OFF);
}
static bool stream_off(uint8_t addr7) {
  return esp32p4_sccb_write8(addr7, OV2710_REG_SW_SLEEP, OV2710_SW_SLEEP_ON) &&
         esp32p4_sccb_write8(addr7, OV2710_REG_FRAME_CTRL01, 0x00) &&
         esp32p4_sccb_write8(addr7, OV2710_REG_FRAME_CTRL02, 0x0f);
}
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)ov2710_mipi_1lane_24Minput_1920x1080_raw10_25fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "OV2710 1920x1080";
    mode_out->width = 1920; mode_out->height = 1080; mode_out->lanes = 1;
    mode_out->lane_mbps = 800; mode_out->in_fmt = ESP32P4_CAM_IN_RAW10; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_1080P; mode_out->regs = regs; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_OV2710, "OV2710", ESP32P4_CAM_SUPPORT_EXPERIMENTAL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *ov2710_sensor_ops(void) { return &ov2710_drv::kOps; }


/* ---- SC202CS pid=0xEB52 ---- */
namespace sc202cs_drv {
#include "cam/sensors/sc202cs/sc202cs_types.h"
#include "cam/sensors/sc202cs/sc202cs_regs.h"
#include "cam/sensors/sc202cs/sc202cs_mipi_1lane_24Minput_1280x720_raw8_30fps.h"
static const uint8_t kAddrs[] = {0x36, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x3107, &got)) continue;
    if (got == 0xEB52) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x01); }
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x00); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)sc202cs_mipi_1lane_24Minput_1280x720_raw8_30fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "SC202CS 1280x720";
    mode_out->width = 1280; mode_out->height = 720; mode_out->lanes = 1;
    mode_out->lane_mbps = 360; mode_out->in_fmt = ESP32P4_CAM_IN_RAW8; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_HD; mode_out->regs = regs; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_SC202CS, "SC202CS", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *sc202cs_sensor_ops(void) { return &sc202cs_drv::kOps; }


/* ---- SC1346 pid=0xDA4D ---- */
namespace sc1346_drv {
#include "cam/sensors/sc1346/sc1346_types.h"
#include "cam/sensors/sc1346/sc1346_regs.h"
#ifndef FRAME_LENGTH_30FPS
#define FRAME_LENGTH_30FPS 900
#endif
#include "cam/sensors/sc1346/sc1346_mipi_1lane_24Minput_720p_raw10_30fps.h"
static const uint8_t kAddrs[] = {0x30, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x3107, &got)) continue;
    if (got == 0xDA4D) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x01); }
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x00); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)sc1346_mipi_1lane_24Minput_720p_raw10_30fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "SC1346 1280x720";
    mode_out->width = 1280; mode_out->height = 720; mode_out->lanes = 1;
    mode_out->lane_mbps = 480; mode_out->in_fmt = ESP32P4_CAM_IN_RAW10; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_HD; mode_out->regs = regs; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_SC1346, "SC1346", ESP32P4_CAM_SUPPORT_FULL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *sc1346_sensor_ops(void) { return &sc1346_drv::kOps; }


/* ---- SC030IOT pid=0x9A46 (8-bit SCCB) ---- */
namespace sc030iot_drv {
#include "cam/sensors/sc030iot/sc030iot_types.h"
#include "cam/sensors/sc030iot/sc030iot_regs.h"
#include "cam/sensors/sc030iot/sc030iot_mipi_1lane_24Minput_640x480_raw8_60fps.h"
static const uint8_t kAddrs[] = {0x68, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    // Page 0x30 then ID high/low at 0xf7/0xf8 (mapped as 0x31f7/0x31f8 in datasheet).
    if (!esp32p4_sccb_write8_reg8(*p, SC030IOT_REG_PAGE_SELECT, 0x30)) continue;
    uint8_t hi = 0, lo = 0;
    if (!esp32p4_sccb_read8_reg8(*p, 0xf7, &hi)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, 0xf8, &lo)) continue;
    uint16_t got = (uint16_t)((hi << 8) | lo);
    if (got == 0x9A46) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, SC030IOT_REG_PAGE_SELECT, 0x31) &&
         esp32p4_sccb_write8_reg8(addr7, 0x00, 0x01);
}
static bool stream_off(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, SC030IOT_REG_PAGE_SELECT, 0x31) &&
         esp32p4_sccb_write8_reg8(addr7, 0x00, 0x00);
}
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8a8_t *regs =
      (const esp32p4_reg8a8_t *)sc030iot_mipi_1lane_24Minput_640x480_raw8_60fps;
  size_t n = sizeof(sc030iot_mipi_1lane_24Minput_640x480_raw8_60fps) /
             sizeof(sc030iot_mipi_1lane_24Minput_640x480_raw8_60fps[0]);
  if (!esp32p4_cam_write_reg8a8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "SC030IOT 640x480";
    mode_out->width = 640; mode_out->height = 480; mode_out->lanes = 1;
    mode_out->lane_mbps = 240; mode_out->in_fmt = ESP32P4_CAM_IN_RAW8; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_VGA; mode_out->regs = nullptr; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_SC030IOT, "SC030IOT", ESP32P4_CAM_SUPPORT_EXPERIMENTAL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *sc030iot_sensor_ops(void) { return &sc030iot_drv::kOps; }


/* ---- OS04C10 pid=0x5304 ---- */
namespace os04c10_drv {
#include "cam/sensors/os04c10/os04c10_types.h"
#include "cam/sensors/os04c10/os04c10_regs.h"
#include "cam/sensors/os04c10/os04c10_mipi_1lane_24Minput_960x1280_raw10_30fps.h"
static const uint8_t kAddrs[] = {0x36, 0x10, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t got = 0;
    if (!esp32p4_sccb_read16(*p, 0x300A, &got)) continue;
    if (got == 0x5304) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x01); }
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x00); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)os04c10_mipi_1lane_24Minput_960x1280_raw10_30fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "OS04C10 960x1280";
    mode_out->width = 960; mode_out->height = 1280; mode_out->lanes = 1;
    mode_out->lane_mbps = 800; mode_out->in_fmt = ESP32P4_CAM_IN_RAW10; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_1080P; mode_out->regs = regs; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_OS04C10, "OS04C10", ESP32P4_CAM_SUPPORT_EXPERIMENTAL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *os04c10_sensor_ops(void) { return &os04c10_drv::kOps; }


/* ---- STI2250 pid=0x2250 (8-bit SCCB) ---- */
namespace sti2250_drv {
#include "cam/sensors/sti2250/sti2250_types.h"
#include "cam/sensors/sti2250/sti2250_regs.h"
#ifndef CONFIG_IDF_TARGET_ESP32P4
#define CONFIG_IDF_TARGET_ESP32P4 1
#endif
#include "cam/sensors/sti2250/sti2250_mipi_1lane_24Minput_raw8_800x600_50fps.h"
static const uint8_t kAddrs[] = {0x37, 0x10, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint8_t hi = 0, lo = 0;
    if (!esp32p4_sccb_read8_reg8(*p, STI2250_REG_CHIP_ID_H, &hi)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, STI2250_REG_CHIP_ID_L, &lo)) continue;
    uint16_t got = (uint16_t)((hi << 8) | lo);
    if (got == 0x2250) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, 0xFD, 0x00) && esp32p4_sccb_write8_reg8(addr7, 0x10, 0x01);
}
static bool stream_off(uint8_t addr7) {
  return esp32p4_sccb_write8_reg8(addr7, 0xFD, 0x00) && esp32p4_sccb_write8_reg8(addr7, 0x10, 0x00);
}
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8a8_t *regs = (const esp32p4_reg8a8_t *)MIPI_1lane_24Minput_RAW8_800x600_50fps;
  size_t n = sizeof(MIPI_1lane_24Minput_RAW8_800x600_50fps) / sizeof(MIPI_1lane_24Minput_RAW8_800x600_50fps[0]);
  if (!esp32p4_cam_write_reg8a8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "STI2250 800x600";
    mode_out->width = 800; mode_out->height = 600; mode_out->lanes = 1;
    mode_out->lane_mbps = 400; mode_out->in_fmt = ESP32P4_CAM_IN_RAW8; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_SVGA; mode_out->regs = nullptr; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_STI2250, "STI2250", ESP32P4_CAM_SUPPORT_EXPERIMENTAL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *sti2250_sensor_ops(void) { return &sti2250_drv::kOps; }


/* ---- MIRA220 pid=0x0130 ---- */
namespace mira220_drv {
#include "cam/sensors/mira220/mira220_types.h"
#include "cam/sensors/mira220/mira220_regs.h"
#include "cam/sensors/mira220/mira220_mipi_2lane_24Minput_1024x600_raw8_15fps.h"
static const uint8_t kAddrs[] = {0x54, 0};
static bool detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint8_t hi = 0, lo = 0;
    if (!esp32p4_sccb_read8(*p, MIRA220_REG_SENSOR_ID_H, &hi)) continue;
    if (!esp32p4_sccb_read8(*p, MIRA220_REG_SENSOR_ID_L, &lo)) continue;
    uint16_t got = ((uint16_t)hi << 8) | lo;
    if (got == 0x0130) { if (addr7_out) *addr7_out = *p; return true; }
  }
  return false;
}
static bool stream_on(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x01); }
static bool stream_off(uint8_t addr7) { return esp32p4_sccb_write8(addr7, 0x0100, 0x00); }
static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  stream_off(addr7);
  delay(5);
  const esp32p4_reg8_t *regs = (const esp32p4_reg8_t *)mira220_mipi_2lane_24Minput_1024x600_raw8_15fps;
  size_t n = esp32p4_cam_reg8_count(regs);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, n)) return false;
  if (mode_out) {
    mode_out->name = "MIRA220 1024x600";
    mode_out->width = 1024; mode_out->height = 600; mode_out->lanes = 2;
    mode_out->lane_mbps = 400; mode_out->in_fmt = ESP32P4_CAM_IN_RAW8; mode_out->bayer = ESP32P4_BAYER_BGGR;
    mode_out->framesize_tag = ESP32P4_FRAMESIZE_SVGA; mode_out->regs = regs; mode_out->regs_count = n;
  }
  return true;
}
static const esp32p4_cam_sensor_ops_t kOps = {
  ESP32P4_SENSOR_MIRA220, "MIRA220", ESP32P4_CAM_SUPPORT_EXPERIMENTAL, kAddrs, detect, configure, stream_on, stream_off,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr
};
}  // namespace
const esp32p4_cam_sensor_ops_t *mira220_sensor_ops(void) { return &mira220_drv::kOps; }
