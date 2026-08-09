#include "storage/ESP32P4_StoragePref.h"

#include <stdio.h>
#include <string.h>

#if __has_include(<FFat.h>)
#include <FFat.h>
#define ESP32P4_HAS_FFAT 1
#else
#define ESP32P4_HAS_FFAT 0
#endif

#if __has_include(<LittleFS.h>)
#include <LittleFS.h>
#define ESP32P4_HAS_LITTLEFS 1
#else
#define ESP32P4_HAS_LITTLEFS 0
#endif

#if __has_include(<SPIFFS.h>)
#include <SPIFFS.h>
#define ESP32P4_HAS_SPIFFS 1
#else
#define ESP32P4_HAS_SPIFFS 0
#endif

const char *ESP32P4_StoragePref::kindName(esp32p4_storage_kind_t k) {
  switch (k) {
    case ESP32P4_STORAGE_SD:
      return "SD";
    case ESP32P4_STORAGE_FFAT:
      return "FFat";
    case ESP32P4_STORAGE_LITTLEFS:
      return "LittleFS";
    case ESP32P4_STORAGE_SPIFFS:
      return "SPIFFS";
    case ESP32P4_STORAGE_AUTO:
    default:
      return "AUTO";
  }
}

const char *ESP32P4_StoragePref::label() const {
  return kindName(_kind == ESP32P4_STORAGE_AUTO ? ESP32P4_STORAGE_SD : _kind);
}

void ESP32P4_StoragePref::clear() {
  _fs = nullptr;
  _kind = ESP32P4_STORAGE_AUTO;
  _owns_sd_begin = false;
  strncpy(_vfs, "/sdcard", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
}

void ESP32P4_StoragePref::end() {
  // Do not unmount flash/SD here — caller owns lifecycle; just drop pointers.
  clear();
  _sd = nullptr;
}

void ESP32P4_StoragePref::applyModelMount() const {
  if (_vfs[0]) esp32p4_set_model_mount_point(_vfs);
}

bool ESP32P4_StoragePref::vfsPath(char *out, size_t out_cap, const char *rel) const {
  if (!out || out_cap < 2 || !rel) return false;
  const char *r = rel;
  while (*r == '/') r++;
  int n = snprintf(out, out_cap, "%s/%s", _vfs, r);
  return n > 0 && (size_t)n < out_cap;
}

bool ESP32P4_StoragePref::exists(const char *path) const {
  return _fs && path && _fs->exists(path);
}

bool ESP32P4_StoragePref::mkdir(const char *path) {
  return _fs && path && _fs->mkdir(path);
}

bool ESP32P4_StoragePref::remove(const char *path) {
  return _fs && path && _fs->remove(path);
}

bool ESP32P4_StoragePref::writeBytes(const char *path, const uint8_t *data, size_t len) {
  if (!_fs || !path || !data) return false;
  File f = _fs->open(path, FILE_WRITE);
  if (!f) return false;
  size_t n = f.write(data, len);
  f.flush();
  f.close();
  return n == len;
}

bool ESP32P4_StoragePref::writeFile(const char *path, const char *text) {
  if (!text) return false;
  return writeBytes(path, (const uint8_t *)text, strlen(text));
}

uint64_t ESP32P4_StoragePref::totalBytes() const {
  if (!_fs) return 0;
  if (_kind == ESP32P4_STORAGE_SD && _sd) return _sd->totalBytes();
#if ESP32P4_HAS_FFAT
  if (_kind == ESP32P4_STORAGE_FFAT) return FFat.totalBytes();
#endif
#if ESP32P4_HAS_LITTLEFS
  if (_kind == ESP32P4_STORAGE_LITTLEFS) return LittleFS.totalBytes();
#endif
#if ESP32P4_HAS_SPIFFS
  if (_kind == ESP32P4_STORAGE_SPIFFS) return SPIFFS.totalBytes();
#endif
  return 0;
}

uint64_t ESP32P4_StoragePref::usedBytes() const {
  if (!_fs) return 0;
  if (_kind == ESP32P4_STORAGE_SD && _sd) return _sd->usedBytes();
#if ESP32P4_HAS_FFAT
  if (_kind == ESP32P4_STORAGE_FFAT) return FFat.usedBytes();
#endif
#if ESP32P4_HAS_LITTLEFS
  if (_kind == ESP32P4_STORAGE_LITTLEFS) return LittleFS.usedBytes();
#endif
#if ESP32P4_HAS_SPIFFS
  if (_kind == ESP32P4_STORAGE_SPIFFS) return SPIFFS.usedBytes();
#endif
  return 0;
}

bool ESP32P4_StoragePref::mountSd(ESP32P4_Sd *sd, esp32p4_board_t board) {
  if (!sd) {
    Serial.println("Storage: SD selected but no ESP32P4_Sd* provided");
    return false;
  }
  if (!sd->mounted()) {
    if (!sd->begin(board)) return false;
    _owns_sd_begin = true;
  }
  _sd = sd;
  _fs = &sd->fs();
  _kind = ESP32P4_STORAGE_SD;
  strncpy(_vfs, "/sdcard", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  return true;
}

bool ESP32P4_StoragePref::mountFFat(bool format_on_fail) {
#if ESP32P4_HAS_FFAT
  if (!FFat.begin(format_on_fail)) {
    Serial.println("Storage: FFat mount FAILED (need FAT partition, e.g. app3M_fat9M_16MB)");
    return false;
  }
  _fs = &FFat;
  _kind = ESP32P4_STORAGE_FFAT;
  _sd = nullptr;
  strncpy(_vfs, "/ffat", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  return true;
#else
  (void)format_on_fail;
  Serial.println("Storage: FFat not available in this core");
  return false;
#endif
}

bool ESP32P4_StoragePref::mountLittleFS(bool format_on_fail) {
#if ESP32P4_HAS_LITTLEFS
  if (!LittleFS.begin(format_on_fail)) {
    Serial.println("Storage: LittleFS mount FAILED");
    return false;
  }
  _fs = &LittleFS;
  _kind = ESP32P4_STORAGE_LITTLEFS;
  _sd = nullptr;
  strncpy(_vfs, "/littlefs", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  return true;
#else
  (void)format_on_fail;
  Serial.println("Storage: LittleFS not available in this core");
  return false;
#endif
}

bool ESP32P4_StoragePref::mountSPIFFS(bool format_on_fail) {
#if ESP32P4_HAS_SPIFFS
  if (!SPIFFS.begin(format_on_fail)) {
    Serial.println("Storage: SPIFFS mount FAILED");
    return false;
  }
  _fs = &SPIFFS;
  _kind = ESP32P4_STORAGE_SPIFFS;
  _sd = nullptr;
  strncpy(_vfs, "/spiffs", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  return true;
#else
  (void)format_on_fail;
  Serial.println("Storage: SPIFFS not available in this core");
  return false;
#endif
}

bool ESP32P4_StoragePref::begin(esp32p4_storage_kind_t pref, bool format_flash_on_fail, ESP32P4_Sd *sd,
                                esp32p4_board_t board) {
  clear();
  _pref = pref;
  _sd = sd;
  bool ok = false;

  switch (pref) {
    case ESP32P4_STORAGE_SD:
      ok = mountSd(sd, board);
      break;
    case ESP32P4_STORAGE_FFAT:
      ok = mountFFat(format_flash_on_fail);
      break;
    case ESP32P4_STORAGE_LITTLEFS:
      ok = mountLittleFS(format_flash_on_fail);
      break;
    case ESP32P4_STORAGE_SPIFFS:
      ok = mountSPIFFS(format_flash_on_fail);
      break;
    case ESP32P4_STORAGE_AUTO:
    default:
      if (mountSd(sd, board)) {
        ok = true;
      } else if (mountFFat(format_flash_on_fail)) {
        ok = true;
      } else if (mountLittleFS(format_flash_on_fail)) {
        ok = true;
      } else if (mountSPIFFS(format_flash_on_fail)) {
        ok = true;
      }
      break;
  }

  if (!ok) {
    Serial.printf("Storage: begin(%s) FAILED\n", kindName(pref));
    clear();
    return false;
  }

  applyModelMount();
  Serial.printf("Storage: using %s  vfs=%s  total=%llu KB used=%llu KB\n", label(), _vfs,
                (unsigned long long)(totalBytes() / 1024ULL),
                (unsigned long long)(usedBytes() / 1024ULL));
  return true;
}
