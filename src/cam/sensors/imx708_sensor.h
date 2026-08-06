#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool imx708_i2c_write8(uint8_t addr7, uint16_t reg, uint8_t val);
bool imx708_i2c_read8(uint8_t addr7, uint16_t reg, uint8_t *val);
bool imx708_i2c_read16(uint8_t addr7, uint16_t reg, uint16_t *val);
bool imx708_write_table(uint8_t addr7, const void *table);
bool imx708_detect(uint8_t *addr7_out);
bool imx708_stream_off(uint8_t addr7);
bool imx708_stream_on(uint8_t addr7);
bool imx708_configure_hd720(uint8_t addr7);
bool imx708_configure_2304x1296(uint8_t addr7);

#ifdef __cplusplus
}
#endif
