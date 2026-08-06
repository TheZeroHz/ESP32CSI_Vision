#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"

/**
 * microSD via SDMMC (not SPI).
 * Guition JC-ESP32P4-M3: CLK=43 CMD=44 D0..D3=39..42, LDO ch4 @ 3.3 V.
 * CSI camera uses LDO ch3 — both can run together.
 */

struct esp32p4_sd_config_t {
  int clk;
  int cmd;
  int d0;
  int d1;
  int d2;
  int d3;
  int ldo_chan;                 // on-chip LDO for SD IO (-1 = leave to core default)
  const char *mountpoint;
  bool mode1bit;
  bool format_if_mount_failed;
  int frequency;                // e.g. SDMMC_FREQ_DEFAULT / SDMMC_FREQ_HIGHSPEED
};

esp32p4_sd_config_t esp32p4_sd_config_default();
esp32p4_sd_config_t esp32p4_sd_config_board(esp32p4_board_t board);

class ESP32P4_Sd {
 public:
  bool begin(esp32p4_board_t board = ESP32P4_BOARD_GUITION_M3);
  bool begin(const esp32p4_sd_config_t &cfg);
  void end();

  bool mounted() const { return _mounted; }
  fs::FS &fs() { return SD_MMC; }

  uint8_t cardType() const;
  uint64_t cardSize() const;
  uint64_t totalBytes() const;
  uint64_t usedBytes() const;

  bool writeFile(const char *path, const char *data);
  bool writeBytes(const char *path, const uint8_t *data, size_t len);
  bool appendFile(const char *path, const char *data);
  bool readFile(const char *path, char *out, size_t out_cap, size_t *out_len = nullptr);
  void listDir(const char *dirname = "/", uint8_t levels = 0);
  bool exists(const char *path);
  bool remove(const char *path);
  bool mkdir(const char *path);
  bool rename(const char *pathFrom, const char *pathTo);

 private:
  bool _mounted = false;
  esp32p4_sd_config_t _cfg{};
};
