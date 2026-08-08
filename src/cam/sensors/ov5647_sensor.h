#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ov5647_detect(uint8_t *addr7_out);
bool ov5647_configure_800x640_raw8(uint8_t addr7);
bool ov5647_stream_restart(uint8_t addr7);
bool ov5647_set_test_pattern(uint8_t addr7, bool enable);
void ov5647_dump_key_regs(uint8_t addr7);

bool ov5647_set_hmirror(uint8_t addr7, bool enable);
bool ov5647_set_vflip(uint8_t addr7, bool enable);
bool ov5647_get_hmirror(uint8_t addr7, bool *out);
bool ov5647_get_vflip(uint8_t addr7, bool *out);

/** aec/agc: true = auto, false = manual (bit0 AEC, bit1 AGC on 0x3503). */
bool ov5647_set_aec(uint8_t addr7, bool enable);
bool ov5647_set_agc(uint8_t addr7, bool enable);
bool ov5647_get_aec(uint8_t addr7, bool *out);
bool ov5647_get_agc(uint8_t addr7, bool *out);

/**
 * Manual exposure in sensor lines; written as (lines << 4) into 0x3500..02.
 * Clamped to mode VTS (800x640 table uses VTS=984) so AEC cannot exceed a frame.
 */
bool ov5647_set_exposure(uint8_t addr7, uint16_t lines);
bool ov5647_get_exposure(uint8_t addr7, uint16_t *lines);
uint16_t ov5647_exposure_max_lines(void);

/** Analogue gain 0..1023 (10-bit) via 0x350A/0x350B. */
bool ov5647_set_gain(uint8_t addr7, uint16_t gain);
bool ov5647_get_gain(uint8_t addr7, uint16_t *gain);

/** Gain ceiling register pair 0x3A18/0x3A19. */
bool ov5647_set_gainceiling(uint8_t addr7, uint16_t ceiling);
bool ov5647_get_gainceiling(uint8_t addr7, uint16_t *ceiling);

#ifdef __cplusplus
}
#endif
