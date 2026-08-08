#pragma once
/**
 * @file WfmStorage.h
 * @brief Pluggable filesystem backends for WebFileManager.
 *
 * Use a concrete backend (SD, SD_MMC, FFat, LittleFS, SPIFFS) or wrap any
 * `fs::FS` with WfmStorageFS + optional size callbacks.
 */
#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * Abstract storage — all backends expose the same surface so the HTTP layer
 * stays DRY.
 */
class WfmStorage {
 public:
  virtual ~WfmStorage() = default;

  /** Mount / initialize the volume. Return false on failure. */
  virtual bool begin() = 0;

  virtual bool mounted() const = 0;
  virtual fs::FS &fs() = 0;

  /** Human label shown in the UI (e.g. "SD", "LittleFS"). */
  virtual const char *label() const = 0;

  /** Best-effort capacity. Return 0 if unknown. May be slow on FAT. */
  virtual uint64_t totalBytes() = 0;
  virtual uint64_t usedBytes() = 0;

  /** Optional card size (SD); defaults to totalBytes(). */
  virtual uint64_t mediaBytes() { return totalBytes(); }

  bool exists(const char *path);
  bool remove(const char *path);
  bool mkdir(const char *path);
  bool rename(const char *from, const char *to);

  /** Serialize FS access across UI + transfer tasks. */
  void lock();
  void unlock();

 protected:
  WfmStorage();
  SemaphoreHandle_t _mu = nullptr;
};

/** Wrap any Arduino `fs::FS` (custom mounts, RamFS, etc.). */
class WfmStorageFS : public WfmStorage {
 public:
  using SizeFn = uint64_t (*)();

  WfmStorageFS(fs::FS &fs, const char *label = "FS", SizeFn totalFn = nullptr,
               SizeFn usedFn = nullptr)
      : _fs(&fs), _label(label), _totalFn(totalFn), _usedFn(usedFn), _ok(false) {}

  bool begin() override {
    _ok = true;
    return true;
  }
  bool mounted() const override { return _ok && _fs; }
  fs::FS &fs() override { return *_fs; }
  const char *label() const override { return _label ? _label : "FS"; }
  uint64_t totalBytes() override { return _totalFn ? _totalFn() : 0; }
  uint64_t usedBytes() override { return _usedFn ? _usedFn() : 0; }

 private:
  fs::FS *_fs;
  const char *_label;
  SizeFn _totalFn;
  SizeFn _usedFn;
  bool _ok;
};

#if __has_include(<SD.h>)
#include <SD.h>
#include <SPI.h>
/** SPI SD card (`SD` library). */
class WfmStorageSD : public WfmStorage {
 public:
  explicit WfmStorageSD(int csPin = SS, SPIClass *spi = &SPI, uint32_t freq = 4000000)
      : _cs(csPin), _spi(spi), _freq(freq), _ok(false) {}
  bool begin() override {
    _ok = _spi ? SD.begin(_cs, *_spi, _freq) : SD.begin(_cs);
    return _ok;
  }
  bool mounted() const override { return _ok; }
  fs::FS &fs() override { return SD; }
  const char *label() const override { return "SD"; }
  uint64_t totalBytes() override { return _ok ? SD.totalBytes() : 0; }
  uint64_t usedBytes() override { return _ok ? SD.usedBytes() : 0; }
  uint64_t mediaBytes() override { return _ok ? SD.cardSize() : 0; }

 private:
  int _cs;
  SPIClass *_spi;
  uint32_t _freq;
  bool _ok;
};
#endif

#if __has_include(<SD_MMC.h>)
#include <SD_MMC.h>
/**
 * SDMMC card. Call setPins() before begin() when your board needs custom pins.
 */
class WfmStorageSD_MMC : public WfmStorage {
 public:
  WfmStorageSD_MMC(const char *mount = "/sdcard", bool mode1bit = false)
      : _mount(mount), _mode1bit(mode1bit), _ok(false) {}

  void setPins(int clk, int cmd, int d0, int d1 = -1, int d2 = -1, int d3 = -1) {
    _clk = clk;
    _cmd = cmd;
    _d0 = d0;
    _d1 = d1;
    _d2 = d2;
    _d3 = d3;
    _pinsSet = true;
  }
  bool begin() override {
    if (_pinsSet) {
      if (_d1 < 0) {
        if (!SD_MMC.setPins(_clk, _cmd, _d0)) return false;
      } else {
        if (!SD_MMC.setPins(_clk, _cmd, _d0, _d1, _d2, _d3)) return false;
      }
    }
    _ok = SD_MMC.begin(_mount, _mode1bit);
    if (_ok && SD_MMC.cardType() == CARD_NONE) {
      SD_MMC.end();
      _ok = false;
    }
    return _ok;
  }
  bool mounted() const override { return _ok; }
  fs::FS &fs() override { return SD_MMC; }
  const char *label() const override { return "SD_MMC"; }
  uint64_t totalBytes() override { return _ok ? SD_MMC.totalBytes() : 0; }
  uint64_t usedBytes() override { return _ok ? SD_MMC.usedBytes() : 0; }
  uint64_t mediaBytes() override { return _ok ? SD_MMC.cardSize() : 0; }

 private:
  const char *_mount;
  bool _mode1bit;
  bool _ok;
  bool _pinsSet = false;
  int _clk = -1, _cmd = -1, _d0 = -1, _d1 = -1, _d2 = -1, _d3 = -1;
};
#endif

#if __has_include(<FFat.h>)
#include <FFat.h>
/** Flash FAT (FFat / wear-leveled). */
class WfmStorageFFat : public WfmStorage {
 public:
  explicit WfmStorageFFat(bool formatOnFail = false) : _format(formatOnFail), _ok(false) {}
  bool begin() override {
    _ok = FFat.begin(_format);
    return _ok;
  }
  bool mounted() const override { return _ok; }
  fs::FS &fs() override { return FFat; }
  const char *label() const override { return "FFat"; }
  uint64_t totalBytes() override { return _ok ? FFat.totalBytes() : 0; }
  uint64_t usedBytes() override { return _ok ? FFat.usedBytes() : 0; }

 private:
  bool _format;
  bool _ok;
};
#endif

#if __has_include(<LittleFS.h>)
#include <LittleFS.h>
/** Flash LittleFS. */
class WfmStorageLittleFS : public WfmStorage {
 public:
  explicit WfmStorageLittleFS(bool formatOnFail = false) : _format(formatOnFail), _ok(false) {}
  bool begin() override {
    _ok = LittleFS.begin(_format);
    return _ok;
  }
  bool mounted() const override { return _ok; }
  fs::FS &fs() override { return LittleFS; }
  const char *label() const override { return "LittleFS"; }
  uint64_t totalBytes() override { return _ok ? LittleFS.totalBytes() : 0; }
  uint64_t usedBytes() override { return _ok ? LittleFS.usedBytes() : 0; }

 private:
  bool _format;
  bool _ok;
};
#endif

#if __has_include(<SPIFFS.h>)
#include <SPIFFS.h>
/** Flash SPIFFS (legacy; prefer LittleFS when possible). */
class WfmStorageSPIFFS : public WfmStorage {
 public:
  explicit WfmStorageSPIFFS(bool formatOnFail = false) : _format(formatOnFail), _ok(false) {}
  bool begin() override {
    _ok = SPIFFS.begin(_format);
    return _ok;
  }
  bool mounted() const override { return _ok; }
  fs::FS &fs() override { return SPIFFS; }
  const char *label() const override { return "SPIFFS"; }
  uint64_t totalBytes() override { return _ok ? SPIFFS.totalBytes() : 0; }
  uint64_t usedBytes() override { return _ok ? SPIFFS.usedBytes() : 0; }

 private:
  bool _format;
  bool _ok;
};
#endif
