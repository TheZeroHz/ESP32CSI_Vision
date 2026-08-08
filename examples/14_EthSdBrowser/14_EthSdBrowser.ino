/**
 * 14_EthSdBrowser - Web file manager over Ethernet + SD
 *
 * Bundled WebFileManager (in ESP32CSI_Vision) + CSI SD helpers.
 * Board: Guition JC-ESP32P4-M3 (IP101 Ethernet + onboard microSD).
 *
 * Open http://<board-ip>/ in a browser to browse, preview, upload,
 * download (ZIP multi-select), create, rename, move, and delete files.
 *
 * Multi-volume paths when LittleFS mounts:
 *   /SD/...       microSD
 *   /LittleFS/... on-chip flash
 *
 * Requires: Arduino-ESP32 3.x, PSRAM enabled, ESP32CSI_Vision only
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

#include <ESP32CSI_Vision.h>
#include <LittleFS.h>

// CSI Vision SD (Guition M3 pins / LDO)
ESP32P4_Sd sd;
WfmStorageFS sdVol(
    sd.fs(), "SD",
    []() -> uint64_t { return sd.totalBytes(); },
    []() -> uint64_t { return sd.usedBytes(); });

// Optional second volume (on-chip flash)
WfmStorageLittleFS flashVol(true);

WfmNetwork net;
WebFileManager wfm(sdVol);

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("=== 14_EthSdBrowser (WebFileManager) ===");

  // --- Storage ---
  if (!sd.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("SD FAILED - check card / format FAT32");
    while (true) delay(1000);
  }
  sdVol.begin();
  Serial.printf("SD card %llu MB\n",
                (unsigned long long)(sd.cardSize() / (1024ULL * 1024ULL)));

  if (flashVol.begin()) {
    wfm.addVolume("LittleFS", flashVol);
    Serial.println("Volumes: /SD and /LittleFS");
  } else {
    Serial.println("Volumes: /SD only (LittleFS not mounted)");
  }

  // --- Network (Ethernet) ---
  net.setHostname("csi-sd-archive");
  if (!net.beginEthernet(WfmNetwork::guitionM3Eth())) {
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
  wfm.loop();  // serve explorer UI + APIs
}
