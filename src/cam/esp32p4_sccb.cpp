#include "cam/esp32p4_sccb.h"

#include <Arduino.h>

static TwoWire *s_sccb = &Wire;

void esp32p4_sccb_set_bus(TwoWire *bus) { s_sccb = bus ? bus : &Wire; }

TwoWire &esp32p4_sccb_bus() { return s_sccb ? *s_sccb : Wire; }

static TwoWire &bus() { return esp32p4_sccb_bus(); }

bool esp32p4_sccb_ping(uint8_t addr7) {
  bus().beginTransmission(addr7);
  return bus().endTransmission() == 0;
}

bool esp32p4_sccb_write8(uint8_t addr7, uint16_t reg, uint8_t val) {
  bus().beginTransmission(addr7);
  bus().write((uint8_t)(reg >> 8));
  bus().write((uint8_t)(reg & 0xff));
  bus().write(val);
  return bus().endTransmission() == 0;
}

bool esp32p4_sccb_read8(uint8_t addr7, uint16_t reg, uint8_t *val) {
  if (!val) return false;
  bus().beginTransmission(addr7);
  bus().write((uint8_t)(reg >> 8));
  bus().write((uint8_t)(reg & 0xff));
  if (bus().endTransmission(false) != 0) return false;
  if (bus().requestFrom((int)addr7, 1) != 1) return false;
  *val = (uint8_t)bus().read();
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
  bus().beginTransmission(addr7);
  bus().write(reg);
  bus().write(val);
  return bus().endTransmission() == 0;
}

bool esp32p4_sccb_read8_reg8(uint8_t addr7, uint8_t reg, uint8_t *val) {
  if (!val) return false;
  bus().beginTransmission(addr7);
  bus().write(reg);
  if (bus().endTransmission(false) != 0) return false;
  if (bus().requestFrom((int)addr7, 1) != 1) return false;
  *val = (uint8_t)bus().read();
  return true;
}
