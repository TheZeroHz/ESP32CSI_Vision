/**
 * 14_EthSdBrowser - Web file manager over Ethernet + multi-volume storage
 *
 * Bundled WebFileManager (in ESP32CSI_Vision) + CSI SD helpers.
 * Board: Guition JC-ESP32P4-M3 (IP101 Ethernet + onboard microSD).
 *
 * Open http://<board-ip>/ in a browser to browse, preview, upload,
 * download (ZIP multi-select), create, rename, move, and delete files.
 *
 * Multi-volume paths when flash mounts:
 *   /SD/...       microSD
 *   /FFat/...     flash FAT (if partition present)
 *   /LittleFS/... on-chip LittleFS
 *
 * Requires: Arduino-ESP32 3.x, PSRAM enabled, ESP32CSI_Vision only
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "14_EthSdBrowser"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_SD | ESP32P4_DBG_NET
#endif

#include <LittleFS.h>

// CSI Vision SD (Guition M3 pins / LDO)
ESP32P4_Sd sd;
WfmStorageFS sdVol(
    sd.fs(), "SD",
    []() -> uint64_t { return sd.totalBytes(); },
    []() -> uint64_t { return sd.usedBytes(); });

// Optional flash volumes
#if __has_include(<FFat.h>)
WfmStorageFFat ffatVol(false);
#endif
WfmStorageLittleFS flashVol(true);

WfmNetwork net;
WebFileManager wfm(sdVol);

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("=== 14_EthSdBrowser (WebFileManager) ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  // --- Storage ---
  if (!sd.begin(esp32csi_sd_config())) {
    Serial.println("SD FAILED - check card / format FAT32");
    while (true) delay(1000);
  }
  sdVol.begin();
  Serial.printf("SD card %llu MB\n",
                (unsigned long long)(sd.cardSize() / (1024ULL * 1024ULL)));

#if __has_include(<FFat.h>)
  if (ffatVol.begin()) {
    wfm.addVolume("FFat", ffatVol);
    Serial.println("Added volume /FFat");
  }
#endif
  if (flashVol.begin()) {
    wfm.addVolume("LittleFS", flashVol);
    Serial.println("Added volume /LittleFS");
  }
  Serial.printf("WFM volumes: %u\n", (unsigned)wfm.volumeCount());

  // --- Network (Ethernet) ---
  net.setHostname("csi-sd-archive");
  if (!net.beginEthernet(esp32csi_wfm_eth())) {
    Serial.println("ETH.begin FAILED - check PHY pins / cable");
    while (true) delay(1000);
  }
  Serial.println("Waiting for DHCP...");
  while (!net.ready()) delay(200);

  // --- WebFileManager ---
  wfm.setName("Media Vault").setPorts(80, 81);
  // Optional HTTP Basic auth:
  // wfm.setAuth("admin", "changeme");
  wfm.begin();
  wfm.startFileTask();  // keep UI responsive during transfers

  Serial.printf("UI        http://%s/\n", net.localIP().toString().c_str());
  Serial.printf("Transfers port %u\n", (unsigned)wfm.filePort());
  Serial.printf("Volumes   %d\n", wfm.volumeCount());
}

void loop() {
  dbg.poll();
  wfm.loop();  // serve explorer UI + APIs
}
