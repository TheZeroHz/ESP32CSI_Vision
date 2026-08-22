#pragma once

/**
 * Runtime VFS mount(s) used by vendored ESP-DL models
 * (e.g. "/sdcard", "/ffat", "/littlefs", "/spiffs").
 *
 * Arduino FS paths stay root-relative ("/models/p4/…"); fopen / ESP-DL use
 * vfsRoot + relative ("/ffat/models/p4/…").
 *
 * Multiple mounts may be registered. locate_* walks them and points
 * esp32p4_model_mount_point() at the volume that actually has the file.
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void esp32p4_clear_model_mounts(void);
void esp32p4_add_model_mount(const char *mount);
void esp32p4_set_model_mount_point(const char *mount);
const char *esp32p4_model_mount_point(void);

/** Rel path without leading slash, e.g. "models/p4/foo.espdl". */
bool esp32p4_locate_rel(const char *rel);
/** Require every rel to exist on the same volume. */
bool esp32p4_locate_rel_all(const char *const *rels, int n);
/** Shortcut for models/p4/<filename>. */
bool esp32p4_locate_models_p4(const char *filename);
bool esp32p4_locate_models_p4_n(const char *const *filenames, int n);

#ifdef __cplusplus
}
#endif
