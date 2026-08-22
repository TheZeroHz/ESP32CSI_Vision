#pragma once

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "sd/ESP32P4_Sd.h"
#include "storage/esp32p4_model_mount.h"

/**
 * User preference for where photos, video, models, face DB, and settings live.
 *
 * begin() mounts the preferred volume as primary, then also mounts any other
 * available volumes (SD, FFat, LittleFS, SPIFFS). Settings/capture stay on
 * primary; models are searched on every mounted volume.
 *
 * Arduino FS paths are always root-relative ("/IMG/…", "/models/p4/…").
 * ESP-DL / fopen need the VFS root ("/sdcard", "/ffat", "/littlefs", "/spiffs").
 *
 * Partition tip: use a scheme with a FAT data partition for FFat
 * (e.g. app3M_fat9M_16MB). Large video is usually happier on microSD.
 */
enum esp32p4_storage_kind_t : uint8_t {
  ESP32P4_STORAGE_AUTO = 0,  // SD → FFat → LittleFS → SPIFFS
  ESP32P4_STORAGE_SD,
  ESP32P4_STORAGE_FFAT,
  ESP32P4_STORAGE_LITTLEFS,
  ESP32P4_STORAGE_SPIFFS,
};

class ESP32P4_StoragePref {
 public:
  /**
   * Mount preferred storage. Pass an ESP32P4_Sd* when SD may be used
   * (owned by caller; begin() is called on it when selecting SD).
   */
  bool begin(esp32p4_storage_kind_t pref = ESP32P4_STORAGE_AUTO,
             bool format_flash_on_fail = false, ESP32P4_Sd *sd = nullptr,
             esp32p4_board_t board = ESP32P4_BOARD_GUITION_M3);
  void end();

  bool ready() const { return _fs != nullptr; }
  esp32p4_storage_kind_t kind() const { return _kind; }
  esp32p4_storage_kind_t preference() const { return _pref; }
  const char *label() const;
  /** VFS mount prefix for fopen / ESP-DL (no trailing slash). */
  const char *vfsRoot() const { return _vfs; }
  fs::FS &fs() { return *_fs; }
  ESP32P4_Sd *sd() { return (_kind == ESP32P4_STORAGE_SD) ? _sd : nullptr; }

  /** Build absolute VFS path: vfsRoot + rel ("/face/db" → "/ffat/face/db"). */
  bool vfsPath(char *out, size_t out_cap, const char *rel) const;

  bool exists(const char *path) const;
  bool mkdir(const char *path);
  bool remove(const char *path);
  bool writeBytes(const char *path, const uint8_t *data, size_t len);
  bool writeFile(const char *path, const char *text);
  uint64_t totalBytes() const;
  uint64_t usedBytes() const;

  /** Mounted volumes (primary first). extras are also registered for model search. */
  int volumeCount() const { return _nvol; }
  const char *volumeLabel(int i) const;
  uint64_t volumeTotal(int i) const;
  uint64_t volumeUsed(int i) const;
  /** "SD+FFat" etc. — valid until the next call. */
  const char *volumeSummary() const;

  /** Point ESP-DL at whichever mounted volume has rel (Arduino FS path). */
  bool locateModel(const char *rel);

  /** Expose extra volumes in WebFileManager (call after constructing WFM). */
  bool attachToWfm(class WebFileManager &wfm);

  /** Re-apply model mount (also done automatically in begin()). */
  void applyModelMount() const;

  static const char *kindName(esp32p4_storage_kind_t k);

 private:
  struct Vol {
    esp32p4_storage_kind_t kind;
    fs::FS *fs;
    char vfs[16];
    char label[12];
  };

  bool mountSd(ESP32P4_Sd *sd, esp32p4_board_t board);
  bool mountFFat(bool format_on_fail);
  bool mountLittleFS(bool format_on_fail);
  bool mountSPIFFS(bool format_on_fail);
  bool mountFlash(bool format_on_fail);
  bool addVol(esp32p4_storage_kind_t kind, fs::FS *fs, const char *vfs, const char *label);
  bool hasKind(esp32p4_storage_kind_t kind) const;
  void mountExtras(bool format_flash_on_fail, ESP32P4_Sd *sd, esp32p4_board_t board);
  void clear();

  esp32p4_storage_kind_t _pref = ESP32P4_STORAGE_AUTO;
  esp32p4_storage_kind_t _kind = ESP32P4_STORAGE_AUTO;
  fs::FS *_fs = nullptr;
  ESP32P4_Sd *_sd = nullptr;
  bool _owns_sd_begin = false;
  bool _sd_attempted = false;
  char _vfs[16] = "/sdcard";
  Vol _vols[4] = {};
  uint8_t _nvol = 0;
};
