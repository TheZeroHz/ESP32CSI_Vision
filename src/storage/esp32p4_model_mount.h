#pragma once

/**
 * Runtime VFS mount used by vendored ESP-DL face models
 * (e.g. "/sdcard", "/ffat", "/littlefs").
 *
 * Arduino FS paths stay root-relative ("/models/p4/…"); fopen / ESP-DL use
 * vfsRoot + relative ("/ffat/models/p4/…").
 */

#ifdef __cplusplus
extern "C" {
#endif

void esp32p4_set_model_mount_point(const char *mount);
const char *esp32p4_model_mount_point(void);

#ifdef __cplusplus
}
#endif
