/**
 * 17_EthH264Record — Ethernet MJPEG UI + SD stills + H.264/MP4 + mic audio
 *
 * Board: Guition JC-ESP32P4-M3 (IP101 Ethernet + CSI + microSD + ES8311 mic)
 *
 * 1. Ethernet + FAT32 microSD
 * 2. Flash (ESP32P4, PSRAM Enabled, Flash 16M)
 * 3. Serial @ 115200 → note IP
 * 4. Browser http://<ip>/
 *      - Live MJPEG + mic waveform
 *      - Capture Img → /IMG/*.jpg
 *      - Record/Stop → /VIDEO/*.mp4 (H.264 + PCM audio fused)
 *
 * Pins (IP101 / RMII): MDC=31 MDIO=52 POWER=51 CLK_IN=50 PHY addr=1
 *
 * H.264 HW encoder is bundled in ESP32CSI_Vision (no separate esp_h264 library).
 */

#include <Arduino.h>

#ifndef ETH_PHY_MDC
#define ETH_PHY_TYPE  ETH_PHY_IP101
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   31
#define ETH_PHY_MDIO  52
#define ETH_PHY_POWER 51
#define ETH_CLK_MODE  EMAC_CLK_EXT_IN
#endif

#include <ETH.h>
#include <ESP32CSI_Vision.h>

static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_H264 h264;
ESP32P4_Mic mic;
ESP32P4_MjpegServer stream;

static volatile bool eth_ready = false;

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-cam");
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
  Serial.println("=== 17_EthH264Record (MJPEG + H.264 + mic) ===");

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!sd.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("SD FAILED — insert FAT32 card and reset");
    while (true) delay(1000);
  }
  Serial.printf("SD ok  %llu MB\n",
                (unsigned long long)(sd.cardSize() / (1024ULL * 1024ULL)));

  if (!h264.begin(ENC_W, ENC_H, 30, ENC_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  if (!mic.begin(16000)) {
    Serial.println("Mic FAILED — recording will be video-only");
  }

  Network.onEvent(onEthEvent);
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER,
                 ETH_CLK_MODE)) {
    Serial.println("ETH.begin FAILED — check cable/PHY pins");
    while (true) delay(1000);
  }

  Serial.println("Waiting for DHCP...");
  uint32_t t0 = millis();
  while (!eth_ready && millis() - t0 < 30000) delay(200);
  if (!eth_ready) {
    Serial.println("no Ethernet IP yet — check cable/router DHCP");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 35)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }

  stream.enableSdCapture(&sd, "/IMG");
  if (!stream.enableVideoRecord(&sd, &h264, "/VIDEO")) {
    Serial.println("enableVideoRecord FAILED");
    while (true) delay(1000);
  }
  if (mic.ready()) stream.enableMic(&mic);

  IPAddress ip = ETH.localIP();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Record → /VIDEO/*.mp4 with fused mic PCM");
}

void loop() {
  stream.loop();
  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("[hb] sent=%u rec=%u videos=%u mic_rms=%.2f last=%s\n",
                  (unsigned)stream.sent(), stream.isRecording() ? 1u : 0u,
                  (unsigned)stream.videosSaved(), mic.ready() ? mic.rms() : 0.0f,
                  stream.lastVideoPath());
  }
}
