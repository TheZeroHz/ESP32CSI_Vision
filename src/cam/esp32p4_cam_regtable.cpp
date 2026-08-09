#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"

#include <Arduino.h>

#define ESP32P4_REG_END 0xFFFFu

static bool is_reg_delay(uint16_t reg) {
  // Unambiguous table sentinels (never used as real 16-bit SCCB addresses here).
  return reg == 0xFFFEu || reg == 0xFEFEu || reg == 0xEEEEu;
}

size_t esp32p4_cam_reg8_count(const esp32p4_reg8_t *regs) {
  if (!regs) return 0;
  size_t n = 0;
  while (regs[n].reg != ESP32P4_REG_END) n++;
  return n;
}

bool esp32p4_cam_write_reg8_table(uint8_t addr7, const esp32p4_reg8_t *regs, size_t n) {
  if (!regs) return false;
  if (!n) n = esp32p4_cam_reg8_count(regs);
  for (size_t i = 0; i < n; i++) {
    if (regs[i].reg == ESP32P4_REG_END) break;
    if (is_reg_delay(regs[i].reg)) {
      delay(regs[i].val ? regs[i].val : 1);
      continue;
    }
    if (!esp32p4_sccb_write8(addr7, regs[i].reg, regs[i].val)) {
      Serial.printf("CSI: reg write fail addr=0x%02X reg=0x%04X\n", addr7, regs[i].reg);
      return false;
    }
  }
  return true;
}

bool esp32p4_cam_write_reg8a8_table(uint8_t addr7, const esp32p4_reg8a8_t *regs, size_t n) {
  if (!regs || !n) return false;
  for (size_t i = 0; i < n; i++) {
    // 8-bit SCCB delay sentinels used by Espressif 8-bit-address sensors.
    if (regs[i].reg == 0xffu || regs[i].reg == 0xf9u) {
      delay(regs[i].val ? regs[i].val : 1);
      continue;
    }
    if (!esp32p4_sccb_write8_reg8(addr7, regs[i].reg, regs[i].val)) {
      Serial.printf("CSI: reg8 write fail addr=0x%02X reg=0x%02X\n", addr7, regs[i].reg);
      return false;
    }
  }
  return true;
}
