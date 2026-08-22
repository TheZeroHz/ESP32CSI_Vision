/**
 * 09_SdCard — SDMMC slot on YOUR carrier.
 *
 * Set CFG_SD_* in board_config.h (docs/Custom-Boards.md).
 * FAT32. Serial @ 115200. 1-bit bus: CFG_SD_1BIT 1.
 */

#ifndef APP_NAME
#define APP_NAME "09_SdCard"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_SD
#endif

#include "board_config.h"

esp32p4_sd_config_t sd_cfg = esp32csi_sd_config();
ESP32P4_Sd sd;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 09_SdCard ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  sd_cfg.frequency = SDMMC_FREQ_DEFAULT;
  esp32csi_print_sd_config(sd_cfg);
  if (!sd.begin(sd_cfg)) {
    Serial.println("sd.begin FAILED — ESP32CSI_SD_* vs schematic, FAT32, LDO");
    while (true) delay(1000);
  }

  Serial.printf("card %llu MB  used %llu / %llu bytes\n",
                (unsigned long long)(sd.cardSize() / (1024ULL * 1024ULL)),
                (unsigned long long)sd.usedBytes(), (unsigned long long)sd.totalBytes());

  sd.listDir("/", 1);

  if (!sd.writeFile("/hello.txt", "Hello from ESP32CSI_Vision\n")) {
    Serial.println("writeFile failed");
  }
  sd.appendFile("/hello.txt", "second line\n");

  char buf[128];
  size_t n = 0;
  if (sd.readFile("/hello.txt", buf, sizeof(buf), &n)) {
    Serial.printf("read %u bytes: %s", (unsigned)n, buf);
  }
}

void loop() {
  dbg.poll();
  delay(2000);
}
