#include "cam/esp32p4_sccb.h"

#include <Wire.h>
#include <Arduino.h>

bool esp32p4_sccb_ping(uint8_t addr7) {
  Wire.beginTransmission(addr7);
  return Wire.endTransmission() == 0;
}

bool esp32p4_sccb_write8(uint8_t addr7, uint16_t reg, uint8_t val) {
  Wire.beginTransmission(addr7);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xff));
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool esp32p4_sccb_read8(uint8_t addr7, uint16_t reg, uint8_t *val) {
  if (!val) return false;
  Wire.beginTransmission(addr7);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xff));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr7, 1) != 1) return false;
  *val = (uint8_t)Wire.read();
  return true;
}

bool esp32p4_sccb_read16(uint8_t addr7, uint16_t reg, uint16_t *val) {
  if (!val) return false;
  uint8_t hi = 0, lo = 0;
  if (!esp32p4_sccb_read8(addr7, reg, &hi)) return false;
  if (!esp32p4_sccb_read8(addr7, (uint16_t)(reg + 1), &lo)) return false;
  *val = (uint16_t)((hi << 8) | lo);
  return true;
}

bool esp32p4_sccb_write8_reg8(uint8_t addr7, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr7);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool esp32p4_sccb_read8_reg8(uint8_t addr7, uint8_t reg, uint8_t *val) {
  if (!val) return false;
  Wire.beginTransmission(addr7);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr7, 1) != 1) return false;
  *val = (uint8_t)Wire.read();
  return true;
}
