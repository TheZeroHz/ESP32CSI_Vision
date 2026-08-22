#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include <Wire.h>

void esp32p4_sccb_set_bus(TwoWire *bus);  // nullptr → Wire
TwoWire &esp32p4_sccb_bus();
/** Bind this camera's I2C for a nested SCCB burst (dual-cam). Pair with unlock. */
void esp32p4_sccb_lock(TwoWire *bus);
void esp32p4_sccb_unlock();

extern "C" {
#endif

bool esp32p4_sccb_write8(uint8_t addr7, uint16_t reg, uint8_t val);
bool esp32p4_sccb_read8(uint8_t addr7, uint16_t reg, uint8_t *val);
bool esp32p4_sccb_read16(uint8_t addr7, uint16_t reg, uint16_t *val);
/** 8-bit register address space (OV-style bank sensors). */
bool esp32p4_sccb_write8_reg8(uint8_t addr7, uint8_t reg, uint8_t val);
bool esp32p4_sccb_read8_reg8(uint8_t addr7, uint8_t reg, uint8_t *val);
/** 16-bit big-endian payload, no register address (DW9714 VCM). */
bool esp32p4_sccb_write_be16(uint8_t addr7, uint16_t val);
bool esp32p4_sccb_ping(uint8_t addr7);

#ifdef __cplusplus
}
#endif
