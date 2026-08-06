#include "mem/ESP32P4_Psram.h"

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

void esp32p4_psram_msync(void *ptr, size_t bytes) {
  if (!ptr || !bytes) return;
  (void)esp_cache_msync(ptr, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}

bool esp32p4_psram_available() {
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM)
  return esp_psram_is_initialized();
#else
  return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0;
#endif
}

size_t esp32p4_psram_free_size() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
