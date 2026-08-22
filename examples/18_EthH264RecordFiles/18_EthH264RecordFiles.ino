/**
 * 18_EthH264RecordFiles — EthH264Record UI + WebFileManager (bidirectional)
 *
 * Board: Guition JC-ESP32P4-M3 (IP101 Ethernet + CSI + microSD + ES8311 mic)
 *
 * Same camera / H.264 / mic features as 17_EthH264Record, plus file browser.
 * APP_STORAGE selects SD / FFat / LittleFS / SPIFFS / AUTO for capture+record.
 * WebFileManager is bundled in ESP32CSI_Vision (no separate library).
 *
 * Ports (same host IP):
 *   :80  Camera UI (EthH264Record)  — click Files → :82
 *   :81  MJPEG /stream
 *   :82  WebFileManager UI          — click Camera → :80
 *   :83  WFM file transfers
 *
 * Avoid browsing/uploading while Record is active (shared volume).
 *
 * Serial @ 115200 · PSRAM on · Arduino-ESP32 3.x
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <Arduino.h>


#include "board_config.h"
#include <ETH.h>

#ifndef APP_NAME
#define APP_NAME "18_EthH264RecordFiles"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE | ESP32P4_DBG_H264 | ESP32P4_DBG_AUDIO
#endif


static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

static const uint16_t CAM_UI_PORT = 80;
static const uint16_t WFM_UI_PORT = 82;
static const uint16_t WFM_FILE_PORT = 83;

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_H264 h264;
ESP32P4_Mic mic;
ESP32P4_MjpegServer stream;

WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

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
  Serial.println("=== 18_EthH264RecordFiles (camera + WebFileManager) ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("APP_STORAGE pref=%s\n", store.kindName(APP_STORAGE));

  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage FAILED - insert SD or use a flash partition");
    while (true) delay(1000);
  }
  static WfmStorageFS primaryVol(
      store.fs(), store.label(), []() -> uint64_t { return store.totalBytes(); },
      []() -> uint64_t { return store.usedBytes(); });
  static WebFileManager wfmMgr(primaryVol);
  primaryVol.begin();
  appVol = &primaryVol;
  wfm = &wfmMgr;
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

  if (!stream.begin(&cam, CAM_UI_PORT, 35)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }

  stream.enableCapture(&store.fs(), "/IMG");
  if (!stream.enableVideoRecord(&store.fs(), &h264, "/VIDEO")) {
    Serial.println("enableVideoRecord FAILED");
    while (true) delay(1000);
  }
  if (mic.ready()) stream.enableMic(&mic);
  stream.setFilesBrowserPort(WFM_UI_PORT);

  wfm->setName("ESP32CSI Files")
      .setPorts(WFM_UI_PORT, WFM_FILE_PORT)
      .setHomePort(CAM_UI_PORT);
  if (!wfm->begin()) {
    Serial.println("WebFileManager begin FAILED");
    while (true) delay(1000);
  }
  wfm->startFileTask();
#if __has_include(<FFat.h>)
  if (store.kind() == ESP32P4_STORAGE_SD) {
    static WfmStorageFFat ffatVol(false);
    if (ffatVol.begin()) {
      wfm->addVolume("FFat", ffatVol);
      Serial.println("WFM: added secondary volume FFat");
    }
  }
#endif
  if (store.kind() != ESP32P4_STORAGE_SD) {
    if (!sd.mounted()) sd.begin(esp32csi_sd_config());
    if (sd.mounted()) {
      static WfmStorageFS sdVol(
          sd.fs(), "SD", []() -> uint64_t { return sd.totalBytes(); },
          []() -> uint64_t { return sd.usedBytes(); });
      sdVol.begin();
      wfm->addVolume("SD", sdVol);
      Serial.println("WFM: added secondary volume SD");
    }
  }

  IPAddress ip = ETH.localIP();
  Serial.printf("Camera  http://%s/          (Files -> :%u)\n", ip.toString().c_str(),
                (unsigned)WFM_UI_PORT);
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.printf("Files   http://%s:%u/       (Camera -> :%u)\n", ip.toString().c_str(),
                (unsigned)WFM_UI_PORT, (unsigned)CAM_UI_PORT);
  Serial.println("Record -> /VIDEO/*.mp4 | Capture -> /IMG/*.jpg");
}

void loop() {
  stream.loop();
  if (wfm && !stream.isRecording()) wfm->loop();

  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("[hb] sent=%u rec=%u videos=%u mic_rms=%.2f last=%s\n",
                  (unsigned)stream.sent(), stream.isRecording() ? 1u : 0u,
                  (unsigned)stream.videosSaved(), mic.ready() ? mic.rms() : 0.0f,
                  stream.lastVideoPath());
  }
}
