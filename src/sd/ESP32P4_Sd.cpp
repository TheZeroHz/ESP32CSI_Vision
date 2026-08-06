#include "sd/ESP32P4_Sd.h"

esp32p4_sd_config_t esp32p4_sd_config_default() {
  return esp32p4_sd_config_board(ESP32P4_BOARD_GUITION_M3);
}

esp32p4_sd_config_t esp32p4_sd_config_board(esp32p4_board_t board) {
  esp32p4_sd_config_t c{};
  // Guition JC-ESP32P4-M3 / Function EV style Slot-0 IO_MUX map
  c.clk = 43;
  c.cmd = 44;
  c.d0 = 39;
  c.d1 = 40;
  c.d2 = 41;
  c.d3 = 42;
  c.ldo_chan = 4;
  c.mountpoint = "/sdcard";
  c.mode1bit = false;
  c.format_if_mount_failed = false;
  c.frequency = SDMMC_FREQ_DEFAULT;
  (void)board;
  return c;
}

bool ESP32P4_Sd::begin(esp32p4_board_t board) { return begin(esp32p4_sd_config_board(board)); }

bool ESP32P4_Sd::begin(const esp32p4_sd_config_t &cfg) {
  end();
  _cfg = cfg;
  if (!_cfg.mountpoint) _cfg.mountpoint = "/sdcard";
  if (_cfg.frequency <= 0) _cfg.frequency = SDMMC_FREQ_DEFAULT;

  if (_cfg.mode1bit) {
    if (!SD_MMC.setPins(_cfg.clk, _cfg.cmd, _cfg.d0)) {
      Serial.println("SD: setPins (1-bit) failed");
      return false;
    }
  } else {
    if (!SD_MMC.setPins(_cfg.clk, _cfg.cmd, _cfg.d0, _cfg.d1, _cfg.d2, _cfg.d3)) {
      Serial.println("SD: setPins (4-bit) failed");
      return false;
    }
  }

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  if (_cfg.ldo_chan >= 0) {
    if (!SD_MMC.setPowerChannel(_cfg.ldo_chan)) {
      Serial.printf("SD: setPowerChannel(%d) failed\n", _cfg.ldo_chan);
      return false;
    }
  }
#endif

  Serial.printf("SD: pins clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d  ldo=%d\n", _cfg.clk, _cfg.cmd,
                _cfg.d0, _cfg.d1, _cfg.d2, _cfg.d3, _cfg.ldo_chan);

  if (!SD_MMC.begin(_cfg.mountpoint, _cfg.mode1bit, _cfg.format_if_mount_failed, _cfg.frequency)) {
    Serial.println("SD: mount FAILED (FAT32? seated? LDO ch4 powered?)");
    return false;
  }

  if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("SD: no card detected");
    SD_MMC.end();
    return false;
  }

  _mounted = true;
  Serial.printf("SD: mounted %s  size=%llu MB  type=%u\n", _cfg.mountpoint,
                (unsigned long long)(cardSize() / (1024ULL * 1024ULL)), (unsigned)cardType());
  return true;
}

void ESP32P4_Sd::end() {
  if (_mounted) {
    SD_MMC.end();
    _mounted = false;
  }
}

uint8_t ESP32P4_Sd::cardType() const { return _mounted ? SD_MMC.cardType() : CARD_NONE; }

uint64_t ESP32P4_Sd::cardSize() const { return _mounted ? SD_MMC.cardSize() : 0; }

uint64_t ESP32P4_Sd::totalBytes() const { return _mounted ? SD_MMC.totalBytes() : 0; }

uint64_t ESP32P4_Sd::usedBytes() const { return _mounted ? SD_MMC.usedBytes() : 0; }

bool ESP32P4_Sd::writeFile(const char *path, const char *data) {
  if (!_mounted || !path || !data) return false;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return false;
  size_t n = f.print(data);
  f.close();
  return n > 0;
}

bool ESP32P4_Sd::writeBytes(const char *path, const uint8_t *data, size_t len) {
  if (!_mounted || !path || !data || !len) return false;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return false;
  size_t n = f.write(data, len);
  f.close();
  return n == len;
}

bool ESP32P4_Sd::appendFile(const char *path, const char *data) {
  if (!_mounted || !path || !data) return false;
  File f = SD_MMC.open(path, FILE_APPEND);
  if (!f) return false;
  size_t n = f.print(data);
  f.close();
  return n > 0;
}

bool ESP32P4_Sd::readFile(const char *path, char *out, size_t out_cap, size_t *out_len) {
  if (!_mounted || !path || !out || out_cap < 1) return false;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return false;
  size_t n = f.readBytes(out, out_cap - 1);
  out[n] = '\0';
  f.close();
  if (out_len) *out_len = n;
  return true;
}

void ESP32P4_Sd::listDir(const char *dirname, uint8_t levels) {
  if (!_mounted || !dirname) return;
  Serial.printf("SD list %s\n", dirname);
  File root = SD_MMC.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("  (open failed)");
    return;
  }
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (f.isDirectory()) {
      Serial.printf("  DIR  %s\n", f.name());
      if (levels) listDir(f.path(), levels - 1);
    } else {
      Serial.printf("  FILE %s  %u bytes\n", f.name(), (unsigned)f.size());
    }
  }
}

bool ESP32P4_Sd::exists(const char *path) { return _mounted && path && SD_MMC.exists(path); }

bool ESP32P4_Sd::remove(const char *path) { return _mounted && path && SD_MMC.remove(path); }

bool ESP32P4_Sd::mkdir(const char *path) { return _mounted && path && SD_MMC.mkdir(path); }

bool ESP32P4_Sd::rename(const char *pathFrom, const char *pathTo) {
  return _mounted && pathFrom && pathTo && SD_MMC.rename(pathFrom, pathTo);
}
