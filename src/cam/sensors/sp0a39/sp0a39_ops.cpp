/*
 * SP0A39 SPI 1-bit gray VGA (Espressif Apache-2.0 tables).
 */
#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"
#include "esp_cam_sensor_types.h"
#include <Arduino.h>

namespace sp0a39_drv {
#include "cam/sensors/sp0a39/sp0a39_types.h"
#include "cam/sensors/sp0a39/sp0a39_regs.h"
#include "cam/sensors/sp0a39/sp0a39_spi_1bit_24Minput_gray_640x480_4fps.h"

#define SP0A39_FRAME_HEADER_SIZE 9
#define SP0A39_LINE_HEADER_SIZE 12
#define SP0A39_FRAME_END_SIZE 4

static const uint8_t kAddrs[] = {0x21, 0};
static const uint8_t kFrameHdr[] = {0xff, 0xff, 0xff, 0x01};
static const uint8_t kLineHdr[] = {0xff, 0xff, 0xff, 0x02};

static const esp_cam_sensor_spi_frame_info kSpiFrame = {
    .frame_size = (640 * 1 + SP0A39_LINE_HEADER_SIZE) * 480 + SP0A39_FRAME_HEADER_SIZE +
                  SP0A39_FRAME_END_SIZE,
    .line_size = (640 * 1 + SP0A39_LINE_HEADER_SIZE),
    .frame_header_check = kFrameHdr,
    .line_header_check = kLineHdr,
    .frame_header_check_size = 4,
    .line_header_check_size = 4,
    .frame_header_size = SP0A39_FRAME_HEADER_SIZE,
    .line_header_size = SP0A39_LINE_HEADER_SIZE,
    .drop_frame_count = 1,
    .high_level_active = 1,
    .data_order_lsb_first = 1,
};

static bool write_table(uint8_t addr7, const sp0a39_reginfo_t *regs, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (regs[i].reg == SP0A39_REG_DELAY) {
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
    uint8_t hi = 0, lo = 0;
    if (!esp32p4_sccb_read8_reg8(*p, SP0A39_REG_CHIP_ID_H, &hi)) continue;
    if (!esp32p4_sccb_read8_reg8(*p, SP0A39_REG_CHIP_ID_L, &lo)) continue;
    if (((uint16_t)((hi << 8) | lo)) == 0x0a39) {
      if (out) *out = *p;
      return true;
    }
  }
  return false;
}

static bool stream_off(uint8_t) { return true; }
static bool stream_on(uint8_t) { return true; }

static bool set_hmirror(uint8_t addr7, bool enable) {
  if (!esp32p4_sccb_write8_reg8(addr7, SP0A39_REG_PAGE_SELECT, 0x00)) return false;
  uint8_t v = 0;
  if (!esp32p4_sccb_read8_reg8(addr7, 0x31, &v)) return false;
  if (enable) v = (uint8_t)(v | (1u << 1));
  else v = (uint8_t)(v & ~(1u << 1));
  return esp32p4_sccb_write8_reg8(addr7, 0x31, v);
}

static bool set_vflip(uint8_t addr7, bool enable) {
  if (!esp32p4_sccb_write8_reg8(addr7, SP0A39_REG_PAGE_SELECT, 0x00)) return false;
  uint8_t v = 0;
  if (!esp32p4_sccb_read8_reg8(addr7, 0x31, &v)) return false;
  if (enable) v = (uint8_t)(v | (1u << 2));
  else v = (uint8_t)(v & ~(1u << 2));
  return esp32p4_sccb_write8_reg8(addr7, 0x31, v);
}

static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  (void)want;
  const size_t n = sizeof(SPI_1bit_24Minput_Gray_640x480_4fps) /
                   sizeof(SPI_1bit_24Minput_Gray_640x480_4fps[0]);
  if (!write_table(addr7, SPI_1bit_24Minput_Gray_640x480_4fps, n)) return false;
  if (!mode_out) return true;
  mode_out->name = "SP0A39 SPI 1-bit 640x480 GRAY8";
  mode_out->width = 640;
  mode_out->height = 480;
  mode_out->lanes = 1;
  mode_out->lane_mbps = 0;
  mode_out->in_fmt = ESP32P4_CAM_IN_GRAY8;
  mode_out->bayer = ESP32P4_BAYER_NONE;
  mode_out->framesize_tag = ESP32P4_FRAMESIZE_VGA;
  mode_out->regs = nullptr;
  mode_out->regs_count = n;
  mode_out->fps = 4;
  mode_out->spi_frame_info = &kSpiFrame;
  return true;
}

static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_SP0A39,
    "SP0A39",
    ESP32P4_CAM_SUPPORT_FULL,
    kAddrs,
    detect,
    configure,
    stream_on,
    stream_off,
    set_hmirror,
    set_vflip,
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
    ESP32P4_CAM_BUS_SPI,
};
}  // namespace sp0a39_drv

const esp32p4_cam_sensor_ops_t *sp0a39_sensor_ops(void) { return &sp0a39_drv::kOps; }
