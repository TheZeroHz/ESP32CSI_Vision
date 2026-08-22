#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef ESP32P4_CACHE_ALIGN
#define ESP32P4_CACHE_ALIGN 128
#endif

void *esp32p4_psram_alloc(size_t bytes, size_t align = ESP32P4_CACHE_ALIGN);
void esp32p4_psram_free(void *ptr);
/** Invalidate CPU cache after DMA write (M2C). Snaps to 128B lines. */
void esp32p4_psram_msync(void *ptr, size_t bytes);
/** Write-back CPU cache before DMA read (C2M). Snaps to 128B lines. */
void esp32p4_psram_writeback(void *ptr, size_t bytes);
bool esp32p4_psram_available();
size_t esp32p4_psram_free_size();
/** Prefer PSRAM for mallocs ≥ 1 KB so internal DRAM stays free for SDMMC/Wi-Fi DMA. */
void esp32p4_prefer_psram();
