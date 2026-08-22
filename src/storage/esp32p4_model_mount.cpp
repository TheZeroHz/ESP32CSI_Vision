#include "storage/esp32p4_model_mount.h"

#include "espdl_arduino_config.h"

#include <stdio.h>
#include <string.h>

#define ESP32P4_MAX_MODEL_MOUNTS 4

static char s_mounts[ESP32P4_MAX_MODEL_MOUNTS][16];
static int s_nmounts = 0;
static int s_active = -1;

static const char *fallback_mount(void) {
#ifdef CONFIG_BSP_SD_MOUNT_POINT
  return CONFIG_BSP_SD_MOUNT_POINT;
#else
  return "/sdcard";
#endif
}

void esp32p4_clear_model_mounts(void) {
  s_nmounts = 0;
  s_active = -1;
}

void esp32p4_add_model_mount(const char *mount) {
  if (!mount || !mount[0]) return;
  for (int i = 0; i < s_nmounts; i++) {
    if (strcmp(s_mounts[i], mount) == 0) return;
  }
  if (s_nmounts >= ESP32P4_MAX_MODEL_MOUNTS) return;
  strncpy(s_mounts[s_nmounts], mount, sizeof(s_mounts[0]) - 1);
  s_mounts[s_nmounts][sizeof(s_mounts[0]) - 1] = '\0';
  s_nmounts++;
}

void esp32p4_set_model_mount_point(const char *mount) {
  if (!mount || !mount[0]) {
    s_active = -1;
    return;
  }
  esp32p4_add_model_mount(mount);
  for (int i = 0; i < s_nmounts; i++) {
    if (strcmp(s_mounts[i], mount) == 0) {
      s_active = i;
      return;
    }
  }
}

const char *esp32p4_model_mount_point(void) {
  if (s_active >= 0 && s_active < s_nmounts && s_mounts[s_active][0]) return s_mounts[s_active];
  if (s_nmounts > 0) return s_mounts[0];
  return fallback_mount();
}

static bool vfs_readable(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  fclose(f);
  return true;
}

static bool mount_has_rel(int i, const char *rel) {
  char path[160];
  int n = snprintf(path, sizeof(path), "%s/%s", s_mounts[i], rel);
  if (n <= 0 || (size_t)n >= sizeof(path)) return false;
  return vfs_readable(path);
}

bool esp32p4_locate_rel(const char *rel) {
  while (rel && *rel == '/') rel++;
  if (!rel || !rel[0]) return false;
  for (int i = 0; i < s_nmounts; i++) {
    if (mount_has_rel(i, rel)) {
      s_active = i;
      return true;
    }
  }
  return false;
}

bool esp32p4_locate_rel_all(const char *const *rels, int n) {
  if (!rels || n <= 0) return false;
  for (int i = 0; i < s_nmounts; i++) {
    bool ok = true;
    for (int k = 0; k < n; k++) {
      const char *rel = rels[k];
      while (rel && *rel == '/') rel++;
      if (!rel || !rel[0] || !mount_has_rel(i, rel)) {
        ok = false;
        break;
      }
    }
    if (ok) {
      s_active = i;
      return true;
    }
  }
  return false;
}

bool esp32p4_locate_models_p4(const char *filename) {
  if (!filename || !filename[0]) return false;
  char rel[96];
  int n = snprintf(rel, sizeof(rel), "models/p4/%s", filename);
  if (n <= 0 || (size_t)n >= sizeof(rel)) return false;
  return esp32p4_locate_rel(rel);
}

bool esp32p4_locate_models_p4_n(const char *const *filenames, int n) {
  if (!filenames || n <= 0) return false;
  if (n == 1) return esp32p4_locate_models_p4(filenames[0]);
  char rels[4][96];
  const char *ptrs[4];
  if (n > 4) n = 4;
  for (int i = 0; i < n; i++) {
    if (!filenames[i] || !filenames[i][0]) return false;
    int w = snprintf(rels[i], sizeof(rels[i]), "models/p4/%s", filenames[i]);
    if (w <= 0 || (size_t)w >= sizeof(rels[i])) return false;
    ptrs[i] = rels[i];
  }
  return esp32p4_locate_rel_all(ptrs, n);
}
