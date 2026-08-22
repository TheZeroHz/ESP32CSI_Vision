#include "storage/ESP32P4_StoragePref.h"

#include "wfm/WebFileManager.h"
#include "wfm/WfmStorage.h"

#include <new>
#include <stdio.h>
#include <string.h>

#define ESP32P4_HAS_FFAT 1
#define ESP32P4_HAS_LITTLEFS 1
#define ESP32P4_HAS_SPIFFS 1

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
  _sd_attempted = false;
  _nvol = 0;
  strncpy(_vfs, "/sdcard", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  memset(_vols, 0, sizeof(_vols));
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
  if (!path) return false;
  for (uint8_t i = 0; i < _nvol; i++) {
    if (_vols[i].fs && _vols[i].fs->exists(path)) return true;
  }
  return _fs && _fs->exists(path);
}

bool ESP32P4_StoragePref::hasKind(esp32p4_storage_kind_t kind) const {
  for (uint8_t i = 0; i < _nvol; i++) {
    if (_vols[i].kind == kind) return true;
  }
  return false;
}

bool ESP32P4_StoragePref::addVol(esp32p4_storage_kind_t kind, fs::FS *fs, const char *vfs,
                                 const char *label) {
  if (!fs || !vfs || !vfs[0] || !label) return false;
  if (hasKind(kind) || _nvol >= 4) return false;
  Vol &v = _vols[_nvol];
  v.kind = kind;
  v.fs = fs;
  strncpy(v.vfs, vfs, sizeof(v.vfs) - 1);
  v.vfs[sizeof(v.vfs) - 1] = '\0';
  strncpy(v.label, label, sizeof(v.label) - 1);
  v.label[sizeof(v.label) - 1] = '\0';
  _nvol++;
  esp32p4_add_model_mount(v.vfs);
  Serial.printf("Storage: volume %s  vfs=%s%s\n", v.label, v.vfs,
                (_nvol == 1) ? " (primary)" : "");
  return true;
}

const char *ESP32P4_StoragePref::volumeLabel(int i) const {
  if (i < 0 || i >= (int)_nvol) return "";
  return _vols[i].label;
}

uint64_t ESP32P4_StoragePref::volumeTotal(int i) const {
  if (i < 0 || i >= (int)_nvol || !_vols[i].fs) return 0;
  switch (_vols[i].kind) {
    case ESP32P4_STORAGE_SD:
      return _sd ? _sd->totalBytes() : 0;
#if ESP32P4_HAS_FFAT
    case ESP32P4_STORAGE_FFAT:
      return FFat.totalBytes();
#endif
#if ESP32P4_HAS_LITTLEFS
    case ESP32P4_STORAGE_LITTLEFS:
      return LittleFS.totalBytes();
#endif
#if ESP32P4_HAS_SPIFFS
    case ESP32P4_STORAGE_SPIFFS:
      return SPIFFS.totalBytes();
#endif
    default:
      return 0;
  }
}

uint64_t ESP32P4_StoragePref::volumeUsed(int i) const {
  if (i < 0 || i >= (int)_nvol || !_vols[i].fs) return 0;
  switch (_vols[i].kind) {
    case ESP32P4_STORAGE_SD:
      return _sd ? _sd->usedBytes() : 0;
#if ESP32P4_HAS_FFAT
    case ESP32P4_STORAGE_FFAT:
      return FFat.usedBytes();
#endif
#if ESP32P4_HAS_LITTLEFS
    case ESP32P4_STORAGE_LITTLEFS:
      return LittleFS.usedBytes();
#endif
#if ESP32P4_HAS_SPIFFS
    case ESP32P4_STORAGE_SPIFFS:
      return SPIFFS.usedBytes();
#endif
    default:
      return 0;
  }
}

const char *ESP32P4_StoragePref::volumeSummary() const {
  static char buf[48];
  buf[0] = '\0';
  for (uint8_t i = 0; i < _nvol; i++) {
    if (i) strncat(buf, "+", sizeof(buf) - 1);
    strncat(buf, _vols[i].label, sizeof(buf) - 1);
  }
  return buf[0] ? buf : label();
}

bool ESP32P4_StoragePref::locateModel(const char *rel) {
  if (!rel) return false;
  while (*rel == '/') rel++;
  return esp32p4_locate_rel(rel);
}

bool ESP32P4_StoragePref::attachToWfm(WebFileManager &wfm) {
  if (_nvol < 2) return false;
  static ESP32P4_StoragePref *s_pref = nullptr;
  s_pref = this;
  static uint8_t mem[3][sizeof(WfmStorageFS)];
  static WfmStorageFS *wrap[3] = {};
  static auto t1 = +[]() -> uint64_t { return s_pref ? s_pref->volumeTotal(1) : 0; };
  static auto u1 = +[]() -> uint64_t { return s_pref ? s_pref->volumeUsed(1) : 0; };
  static auto t2 = +[]() -> uint64_t { return s_pref ? s_pref->volumeTotal(2) : 0; };
  static auto u2 = +[]() -> uint64_t { return s_pref ? s_pref->volumeUsed(2) : 0; };
  static auto t3 = +[]() -> uint64_t { return s_pref ? s_pref->volumeTotal(3) : 0; };
  static auto u3 = +[]() -> uint64_t { return s_pref ? s_pref->volumeUsed(3) : 0; };
  WfmStorageFS::SizeFn totals[3] = {t1, t2, t3};
  WfmStorageFS::SizeFn useds[3] = {u1, u2, u3};
  bool added = false;
  for (uint8_t i = 1; i < _nvol && (i - 1) < 3; i++) {
    const uint8_t wi = (uint8_t)(i - 1);
    if (!wrap[wi]) {
      wrap[wi] = new (mem[wi]) WfmStorageFS(*_vols[i].fs, _vols[i].label, totals[wi], useds[wi]);
    }
    wrap[wi]->begin();
    wfm.addVolume(_vols[i].label, *wrap[wi]);
    Serial.printf("WFM: added volume %s\n", _vols[i].label);
    added = true;
  }
  return added;
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
  _sd_attempted = true;
  if (!sd) {
    if (_pref == ESP32P4_STORAGE_SD) {
      Serial.println("Storage: SD selected but no ESP32P4_Sd* provided");
    }
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
  addVol(ESP32P4_STORAGE_SD, _fs, _vfs, "SD");
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
  strncpy(_vfs, "/ffat", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  addVol(ESP32P4_STORAGE_FFAT, _fs, _vfs, "FFat");
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
  strncpy(_vfs, "/littlefs", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  addVol(ESP32P4_STORAGE_LITTLEFS, _fs, _vfs, "LittleFS");
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
  strncpy(_vfs, "/spiffs", sizeof(_vfs) - 1);
  _vfs[sizeof(_vfs) - 1] = '\0';
  addVol(ESP32P4_STORAGE_SPIFFS, _fs, _vfs, "SPIFFS");
  return true;
#else
  (void)format_on_fail;
  Serial.println("Storage: SPIFFS not available in this core");
  return false;
#endif
}

bool ESP32P4_StoragePref::mountFlash(bool format_on_fail) {
  if (mountFFat(format_on_fail)) return true;
  if (mountLittleFS(false)) return true;
  if (mountSPIFFS(false)) return true;
  Serial.println("Storage: flash empty — formatting LittleFS on data partition (no SD)");
  return mountLittleFS(true);
}

bool ESP32P4_StoragePref::begin(esp32p4_storage_kind_t pref, bool format_flash_on_fail, ESP32P4_Sd *sd,
                                esp32p4_board_t board) {
  clear();
  _pref = pref;
  _sd = sd;
  esp32p4_clear_model_mounts();
  bool ok = false;

  switch (pref) {
    case ESP32P4_STORAGE_SD:
      ok = mountSd(sd, board);
      if (!ok) {
        Serial.println("Storage: no SD card — falling back to flash");
        ok = mountFlash(false);
      }
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
      } else {
        Serial.println("Storage: no SD card — trying flash");
        ok = mountFlash(false);
      }
      break;
  }

  if (!ok) {
    Serial.printf("Storage: begin(%s) FAILED\n", kindName(pref));
    clear();
    return false;
  }

  mountExtras(false, sd, board);
  applyModelMount();
  Serial.printf("Storage: using %s  vfs=%s  volumes=%s  total=%llu KB used=%llu KB\n", label(), _vfs,
                volumeSummary(), (unsigned long long)(totalBytes() / 1024ULL),
                (unsigned long long)(usedBytes() / 1024ULL));
  return true;
}

void ESP32P4_StoragePref::mountExtras(bool format_flash_on_fail, ESP32P4_Sd *sd,
                                      esp32p4_board_t board) {
  (void)format_flash_on_fail;
  if (!hasKind(ESP32P4_STORAGE_SD) && sd && !_sd_attempted) {
    if (!sd->mounted()) sd->begin(board);
    if (sd->mounted()) {
      _sd = sd;
      addVol(ESP32P4_STORAGE_SD, &sd->fs(), "/sdcard", "SD");
    }
  }
#if ESP32P4_HAS_FFAT
  if (!hasKind(ESP32P4_STORAGE_FFAT)) {
    if (FFat.begin(false)) addVol(ESP32P4_STORAGE_FFAT, &FFat, "/ffat", "FFat");
  }
#endif
#if ESP32P4_HAS_LITTLEFS
  if (!hasKind(ESP32P4_STORAGE_LITTLEFS) && !hasKind(ESP32P4_STORAGE_SPIFFS)) {
    if (LittleFS.begin(false)) addVol(ESP32P4_STORAGE_LITTLEFS, &LittleFS, "/littlefs", "LittleFS");
  }
#endif
#if ESP32P4_HAS_SPIFFS
  if (!hasKind(ESP32P4_STORAGE_SPIFFS) && !hasKind(ESP32P4_STORAGE_LITTLEFS)) {
    if (SPIFFS.begin(false)) addVol(ESP32P4_STORAGE_SPIFFS, &SPIFFS, "/spiffs", "SPIFFS");
  }
#endif
}
