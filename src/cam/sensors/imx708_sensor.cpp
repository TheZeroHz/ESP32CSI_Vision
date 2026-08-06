#include "cam/sensors/imx708_sensor.h"

#include <Arduino.h>
#include <Wire.h>

#include "cam/sensors/imx708_regs.h"
#include "cam/sensors/imx708_settings.h"

static const uint8_t kProbeAddrs[] = {0x1A, 0x10};

bool imx708_i2c_write8(uint8_t addr7, uint16_t reg, uint8_t val) {
  Wire.beginTransmission(addr7);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool imx708_i2c_read8(uint8_t addr7, uint16_t reg, uint8_t *val) {
  Wire.beginTransmission(addr7);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission() != 0) return false;
  delayMicroseconds(50);
  if (Wire.requestFrom((int)addr7, 1) != 1) return false;
  *val = (uint8_t)Wire.read();
  return true;
}

bool imx708_i2c_read16(uint8_t addr7, uint16_t reg, uint16_t *val) {
  Wire.beginTransmission(addr7);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission() != 0) return false;
  delayMicroseconds(50);
  if (Wire.requestFrom((int)addr7, 2) != 2) return false;
  uint16_t hi = (uint16_t)Wire.read();
  uint16_t lo = (uint16_t)Wire.read();
  *val = (uint16_t)((hi << 8) | lo);
  return true;
}

bool imx708_write_table(uint8_t addr7, const void *table) {
  const imx708_reginfo_t *t = (const imx708_reginfo_t *)table;
  for (size_t i = 0; t[i].reg != IMX708_REG_END; i++) {
    if (t[i].reg == IMX708_REG_DELAY) {
      delay(t[i].val ? t[i].val : 10);
      continue;
    }
    if (!imx708_i2c_write8(addr7, t[i].reg, t[i].val)) return false;
  }
  return true;
}

bool imx708_detect(uint8_t *addr7_out) {
  for (size_t i = 0; i < sizeof(kProbeAddrs); i++) {
    uint16_t id = 0;
    if (!imx708_i2c_read16(kProbeAddrs[i], IMX708_REG_CHIP_ID, &id)) continue;
    if (id == IMX708_CHIP_ID) {
      *addr7_out = kProbeAddrs[i];
      return true;
    }
  }
  return false;
}

bool imx708_stream_off(uint8_t addr7) {
  return imx708_i2c_write8(addr7, IMX708_REG_MODE_SELECT, IMX708_MODE_STANDBY);
}

bool imx708_stream_on(uint8_t addr7) {
  return imx708_i2c_write8(addr7, IMX708_REG_MODE_SELECT, IMX708_MODE_STREAMING);
}

static bool configure_common(uint8_t addr7) {
  if (!imx708_stream_off(addr7)) return false;
  delay(10);
  if (!imx708_write_table(addr7, imx708_init_reg_tbl)) return false;
  if (!imx708_write_table(addr7, imx708_link_freq_450mhz)) return false;
  return true;
}

bool imx708_configure_hd720(uint8_t addr7) {
  if (!configure_common(addr7)) return false;
  if (!imx708_write_table(addr7, imx708_1280x720_regs)) return false;
  delay(20);
  return imx708_stream_on(addr7);
}

bool imx708_configure_2304x1296(uint8_t addr7) {
  if (!configure_common(addr7)) return false;
  if (!imx708_write_table(addr7, imx708_2304x1296_regs)) return false;
  delay(20);
  return imx708_stream_on(addr7);
}
