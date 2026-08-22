/**
 * 17_EthH264Record — Ethernet MJPEG UI + stills + H.264/MP4 + mic audio
 *
 * Board: Guition JC-ESP32P4-M3 (IP101 Ethernet + CSI + microSD + ES8311 mic)
 *
 * APP_STORAGE: AUTO / SD / FFAT / LITTLEFS / SPIFFS (large video prefers SD).
 *
 * 1. Ethernet + preferred storage (SD or flash partition)
 * 2. Flash (ESP32P4, PSRAM Enabled, Flash 16M, FAT partition if using FFat)
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

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <Arduino.h>


#include "board_config.h"
#include <ETH.h>

#ifndef APP_NAME
#define APP_NAME "17_EthH264Record"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE | ESP32P4_DBG_H264 | ESP32P4_DBG_AUDIO
#endif


static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
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

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 17_EthH264Record (MJPEG + H.264 + mic) ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("APP_STORAGE pref=%s\n", store.kindName(APP_STORAGE));

  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage FAILED - insert SD or use a flash FAT/LittleFS partition");
    while (true) delay(1000);
  }
  Serial.printf("Storage %s  total=%llu KB\n", store.label(),
                (unsigned long long)(store.totalBytes() / 1024ULL));

  if (!h264.begin(ENC_W, ENC_H, 30, ENC_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  if (!mic.begin(16000)) {
    Serial.println("Mic FAILED - recording will be video-only");
  }

  Network.onEvent(onEthEvent);
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER,
                 ETH_CLK_MODE)) {
    Serial.println("ETH.begin FAILED - check cable/PHY pins");
    while (true) delay(1000);
  }

  Serial.println("Waiting for DHCP...");
  uint32_t t0 = millis();
  while (!eth_ready && millis() - t0 < 30000) delay(200);
  if (!eth_ready) {
    Serial.println("no Ethernet IP yet - check cable/router DHCP");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 35)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }

  stream.enableCapture(&store.fs(), "/IMG");
  if (!stream.enableVideoRecord(&store.fs(), &h264, "/VIDEO")) {
    Serial.println("enableVideoRecord FAILED");
    while (true) delay(1000);
  }
  if (mic.ready()) stream.enableMic(&mic);

  IPAddress ip = ETH.localIP();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Record -> /VIDEO/*.mp4 with fused mic PCM");
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
