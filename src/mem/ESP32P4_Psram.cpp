#include "mem/ESP32P4_Psram.h"

#include <stdint.h>
#include <string.h>

#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

void *esp32p4_psram_alloc(size_t bytes, size_t align) {
  if (!bytes) return nullptr;
  if (align < ESP32P4_CACHE_ALIGN) align = ESP32P4_CACHE_ALIGN;
  bytes = (bytes + align - 1) & ~(align - 1);
  void *p = heap_caps_aligned_alloc(align, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_aligned_alloc(align, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (p) memset(p, 0, bytes);
  return p;
}

void esp32p4_psram_free(void *ptr) {
  if (ptr) heap_caps_free(ptr);
}

static void cache_sync_range(void *ptr, size_t bytes, uint32_t flags) {
  if (!ptr || !bytes) return;
  const uintptr_t mask = (uintptr_t)(ESP32P4_CACHE_ALIGN - 1);
  uintptr_t a = (uintptr_t)ptr;
  uintptr_t start = a & ~mask;
  uintptr_t end = (a + bytes + mask) & ~mask;
#ifdef ESP_CACHE_MSYNC_FLAG_TYPE_DATA
  flags |= ESP_CACHE_MSYNC_FLAG_TYPE_DATA;
#endif
  /* M2C rejects UNALIGNED — only C2M may use it, and we already snapped to 128B. */
  flags &= ~ESP_CACHE_MSYNC_FLAG_UNALIGNED;
  (void)esp_cache_msync((void *)start, (size_t)(end - start), flags);
}

void esp32p4_psram_msync(void *ptr, size_t bytes) {
  cache_sync_range(ptr, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}

void esp32p4_psram_writeback(void *ptr, size_t bytes) {
  cache_sync_range(ptr, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}

bool esp32p4_psram_available() {
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM)
  return esp_psram_is_initialized();
#else
  return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0;
#endif
}

size_t esp32p4_psram_free_size() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

void esp32p4_prefer_psram() {
  if (!esp32p4_psram_available()) return;
  // Allocations ≥ 1 KB go to PSRAM when possible. SDMMC + ESP-Hosted Wi-Fi DMA
  // must stay in internal DRAM; the default 16 KB threshold starves them.
  heap_caps_malloc_extmem_enable(1024);
}
