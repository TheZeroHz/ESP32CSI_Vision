/**
 * 00_BoardConfig — print YOUR wiring, then try the camera
 *
 * This library is not tied to one vendor. Copy GPIOs from your schematic into
 * board_config.h (tab next to this sketch; see docs/Custom-Boards.md), flash, and
 * read Serial @ 115200. Lines starting with CFG: are the pins this sketch uses.
 *
 * Next:
 *   01_CamTest          camera only
 *   04_WiFiMjpeg        camera + C6 Wi-Fi
 *   09_SdCard           microSD
 *   15_MicSdRecord      ES8311 mic
 *   44_AvSdRecord       CSI + mic → MP4 (no stream)
 *   30_EthLiveAvFiles   Ethernet
 *   43_CamWebModels     you pass camera_fb_t into a preview + model JSON
 */

#ifndef APP_NAME
#define APP_NAME "00_BoardConfig"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif

#include "board_config.h"

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_Mic mic;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 00_BoardConfig ===");
  Serial.println("Edit board_config.h (this folder) for YOUR ESP32-P4 carrier.");
  dbg.begin(APP_NAME, APP_DEBUG);

  esp32csi_print_board();

  esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
  if (!cam.begin(cam_cfg)) {
    Serial.println("camera begin FAILED — fix CFG_CAM_* in board_config.h then reflash");
  } else {
    Serial.printf("camera OK  %s  %ux%u  %s\n", cam.sensorName(), cam.width(), cam.height(),
                  cam.formatName());
  }

  esp32p4_sd_config_t sd_cfg = esp32csi_sd_config();
  if (!sd.begin(sd_cfg)) {
    Serial.println("sd.begin skipped/FAILED — normal if the slot is empty or pins differ");
  }

  esp32p4_mic_config_t mic_cfg = esp32csi_mic_config();
  if (!mic.begin(mic_cfg)) {
    Serial.println("mic.begin skipped/FAILED — normal if there is no ES8311 on those pins");
  }

  Serial.println("Done. Wi-Fi / ETH are not started here (see 04 / 30).");
}

void loop() {
  dbg.poll();
  delay(2000);
}
