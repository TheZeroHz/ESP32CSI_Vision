/**
 * 20_EthCvPreview — Ethernet MJPEG + CV dashboard
 *
 * Board: Guition JC-ESP32P4-M3 (IP101 Ethernet + CSI)
 *
 * Open http://<ip>/ → panel "CV dashboard":
 *   Mode Mask   — green = HSV match (best first test)
 *   Mode Blobs  — boxes on matching regions
 *   Mode Edges / Threshold / Gray / Blur — other ESP32P4_Cv tests
 *   Preset Any / Red / Green / Blue / Yellow + live HSV sliders
 *
 * Tip: start Mask + Any. If mask_px stays ~0, lower S lo / V lo.
 * Then switch to Blobs.
 */

#include <Arduino.h>


#include "board_config.h"
#include <ETH.h>

#ifndef APP_NAME
#define APP_NAME "20_EthCvPreview"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif


ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Debug dbg;

static volatile bool eth_ready = false;

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-cv");
      Serial.println("ETH Started");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Link Up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("ETH Got IP");
      Serial.println(ETH);
      eth_ready = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      eth_ready = false;
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 20_EthCvPreview (CV dashboard + Ethernet MJPEG) ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  Network.onEvent(onEthEvent);
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER,
                 ETH_CLK_MODE)) {
    Serial.println("ETH.begin FAILED");
    while (true) delay(1000);
  }

  Serial.println("Waiting for DHCP...");
  uint32_t t0 = millis();
  while (!eth_ready && millis() - t0 < 30000) delay(200);
  if (!eth_ready) {
    Serial.println("no Ethernet IP");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 38)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }
  // QVGA→160×128 (JPEG/PPA aligned). Prefer 400×320 for more detail.
  stream.setFramesize(ESP32P4_STREAM_VGA);  // 400×320 exact ½ of 800×640
  stream.enableCvDashboard(true);  // Edge track mode by default

  IPAddress ip = ETH.localIP();
  Serial.printf("UI      http://%s/   (CV: Edge track)\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Mode=Edge track -> cyan ID# boxes. Move objects to see IDs stick.");
}

void loop() {
  stream.loop();
  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    const auto &cv = stream.cvConfig();
    Serial.printf("[hb] sent=%u jpeg=%u mode=%u blobs=%d mask_px=%d cv_ms=%d\n",
                  (unsigned)stream.sent(), (unsigned)stream.lastJpegBytes(),
                  (unsigned)cv.mode, (int)cv.blobs, (int)cv.mask_px, (int)cv.proc_ms);
  }
}
