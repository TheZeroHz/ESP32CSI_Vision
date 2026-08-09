#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool esp32p4_sccb_write8(uint8_t addr7, uint16_t reg, uint8_t val);
bool esp32p4_sccb_read8(uint8_t addr7, uint16_t reg, uint8_t *val);
bool esp32p4_sccb_read16(uint8_t addr7, uint16_t reg, uint16_t *val);
/** 8-bit register address space (OV-style bank sensors). */
bool esp32p4_sccb_write8_reg8(uint8_t addr7, uint8_t reg, uint8_t val);
bool esp32p4_sccb_read8_reg8(uint8_t addr7, uint8_t reg, uint8_t *val);
bool esp32p4_sccb_ping(uint8_t addr7);

#ifdef __cplusplus
}
#endif
