#include "storage/esp32p4_model_mount.h"

#include "espdl_arduino_config.h"

static const char *s_mount = nullptr;

void esp32p4_set_model_mount_point(const char *mount) {
  s_mount = (mount && mount[0]) ? mount : nullptr;
}

const char *esp32p4_model_mount_point(void) {
  if (s_mount && s_mount[0]) return s_mount;
#ifdef CONFIG_BSP_SD_MOUNT_POINT
  return CONFIG_BSP_SD_MOUNT_POINT;
#else
  return "/sdcard";
#endif
}
