/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD (register tables)
 * SPDX-License-Identifier: Apache-2.0
 * Arduino wrapper: ESP32CSI_Vision
 */
#include "cam/sensors/sc2336_sensor.h"
#include "cam/esp32p4_sccb.h"

#include <Arduino.h>
#include <string.h>

#include "cam/sensors/sc2336/sc2336_types.h"
#include "cam/sensors/sc2336/sc2336_regs.h"

namespace {
#include "cam/sensors/sc2336/sc2336_mipi_2lane_24Minput_1280x720_raw10_30fps.h"
#include "cam/sensors/sc2336/sc2336_mipi_2lane_24Minput_1920x1080_raw10_30fps.h"
}  // namespace

static const uint8_t kAddrs[] = {0x30, 0};

static bool sc2336_detect(uint8_t *addr7_out) {
  for (const uint8_t *p = kAddrs; *p; p++) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t pid = 0;
    if (!esp32p4_sccb_read16(*p, SC2336_REG_SENSOR_ID_H, &pid)) continue;
    if (pid == 0xcb3a) {
      if (addr7_out) *addr7_out = *p;
      return true;
    }
  }
  return false;
}

static bool sc2336_stream_off(uint8_t addr7) {
  return esp32p4_sccb_write8(addr7, SC2336_REG_SLEEP_MODE, 0x00);
}

static bool sc2336_stream_on(uint8_t addr7) {
  return esp32p4_sccb_write8(addr7, SC2336_REG_SLEEP_MODE, 0x01);
}

static bool sc2336_configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  const bool want1080 = (want == ESP32P4_FRAMESIZE_AUTO || want == ESP32P4_FRAMESIZE_1080P);
  const esp32p4_reg8_t *regs = nullptr;
  esp32p4_cam_mode_t m{};
  m.bayer = ESP32P4_BAYER_BGGR;
  m.in_fmt = ESP32P4_CAM_IN_RAW10;
  m.lanes = 2;
  if (want1080) {
    regs = (const esp32p4_reg8_t *)sc2336_mipi_2lane_24Minput_1920x1080_raw10_30fps;
    m.name = "SC2336 1920x1080 RAW10 30fps";
    m.width = 1920;
    m.height = 1080;
    m.lane_mbps = 405;
    m.framesize_tag = ESP32P4_FRAMESIZE_1080P;
  } else {
    regs = (const esp32p4_reg8_t *)sc2336_mipi_2lane_24Minput_1280x720_raw10_30fps;
    m.name = "SC2336 1280x720 RAW10 30fps";
    m.width = 1280;
    m.height = 720;
    m.lane_mbps = 405;
    m.framesize_tag = ESP32P4_FRAMESIZE_HD;
  }
  m.regs = regs;
  m.regs_count = esp32p4_cam_reg8_count(regs);
  sc2336_stream_off(addr7);
  delay(5);
  if (!esp32p4_cam_write_reg8_table(addr7, regs, m.regs_count)) return false;
  if (mode_out) *mode_out = m;
  return true;
}

static bool sc2336_set_hmirror(uint8_t addr7, bool en) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(addr7, SC2336_REG_FLIP_MIRROR, &v)) return false;
  if (en) v |= 0x06;
  else v &= (uint8_t)~0x06;
  return esp32p4_sccb_write8(addr7, SC2336_REG_FLIP_MIRROR, v);
}

static bool sc2336_set_vflip(uint8_t addr7, bool en) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(addr7, SC2336_REG_FLIP_MIRROR, &v)) return false;
  if (en) v |= 0x60;
  else v &= (uint8_t)~0x60;
  return esp32p4_sccb_write8(addr7, SC2336_REG_FLIP_MIRROR, v);
}

static bool sc2336_get_hmirror(uint8_t addr7, bool *out) {
  uint8_t v = 0;
  if (!out || !esp32p4_sccb_read8(addr7, SC2336_REG_FLIP_MIRROR, &v)) return false;
  *out = (v & 0x06) != 0;
  return true;
}

static bool sc2336_get_vflip(uint8_t addr7, bool *out) {
  uint8_t v = 0;
  if (!out || !esp32p4_sccb_read8(addr7, SC2336_REG_FLIP_MIRROR, &v)) return false;
  *out = (v & 0x60) != 0;
  return true;
}

static bool sc2336_set_test_pattern(uint8_t addr7, bool en) {
  uint8_t v = 0;
  if (!esp32p4_sccb_read8(addr7, 0x4501, &v)) return false;
  if (en) v |= (1u << 3);
  else v &= (uint8_t)~(1u << 3);
  return esp32p4_sccb_write8(addr7, 0x4501, v);
}

static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_SC2336,
    "SC2336",
    ESP32P4_CAM_SUPPORT_FULL,
    kAddrs,
    sc2336_detect,
    sc2336_configure,
    sc2336_stream_on,
    sc2336_stream_off,
    sc2336_set_hmirror,
    sc2336_set_vflip,
    sc2336_get_hmirror,
    sc2336_get_vflip,
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
    sc2336_set_test_pattern,
};

const esp32p4_cam_sensor_ops_t *sc2336_sensor_ops(void) { return &kOps; }
