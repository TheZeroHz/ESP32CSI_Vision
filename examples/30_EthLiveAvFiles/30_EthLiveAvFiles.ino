/**
 * 30_EthLiveAvFiles — Ethernet live video + browser audio + FileMgr
 *
 * Camera / SD / mic / ETH: board_config.h (docs/Custom-Boards.md).
 * Change ETH_PHY_TYPE before ETH.h if your PHY is not IP101.
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#ifndef APP_NAME
#define APP_NAME "30_EthLiveAvFiles"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

#include <Arduino.h>
#include "board_config.h"
#include <ETH.h>

static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

static const uint16_t CAM_UI_PORT = 80;
static const uint16_t WFM_UI_PORT = 82;
static const uint16_t WFM_FILE_PORT = 83;
static const uint16_t AUDIO_STREAM_PORT = 84;

esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
esp32p4_sd_config_t sd_cfg = esp32csi_sd_config();
esp32p4_mic_config_t mic_cfg = esp32csi_mic_config();
esp32csi_eth_config_t eth_cfg = esp32csi_eth_config();

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
      ETH.setHostname(eth_cfg.hostname);
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
  Serial.println("=== 30_EthLiveAvFiles ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("board=%s storage=%s\n", esp32csi_board_name(),
                store.kindName(APP_STORAGE));
  esp32csi_print_cam_config(cam_cfg);
  esp32csi_print_sd_config(sd_cfg);
  esp32csi_print_mic_config(mic_cfg);
  esp32csi_print_eth_config(eth_cfg);

  cam_cfg.fb_count = 3;
  mic_cfg.sample_rate = 16000;

  if (!cam.begin(cam_cfg)) {
    Serial.println("camera FAILED - check ESP32CSI_CAM_*");
    while (true) delay(1000);
  }

  if (!sd.begin(sd_cfg)) {
    Serial.println("sd.begin(cfg) FAILED - insert FAT32 TF card");
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
  Serial.printf("Storage %s total=%llu KB\n", store.label(),
                (unsigned long long)(store.totalBytes() / 1024ULL));

  if (!h264.begin(ENC_W, ENC_H, 30, ENC_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  if (!mic.begin(mic_cfg)) {
    Serial.println("Mic FAILED - browser audio and MP4 audio will be unavailable");
  }

  Network.onEvent(onEthEvent);
  if (!ETH.begin(ETH_PHY_TYPE, eth_cfg.phy_addr, eth_cfg.mdc, eth_cfg.mdio, eth_cfg.power,
                 ETH_CLK_MODE)) {
    Serial.println("ETH.begin FAILED - ESP32CSI_ETH_* / ETH_PHY_TYPE");
    while (true) delay(1000);
  }

  Serial.println("Waiting for DHCP...");
  uint32_t t0 = millis();
  while (!eth_ready && millis() - t0 < 30000) delay(200);
  if (!eth_ready) {
    Serial.println("no Ethernet IP yet - check cable/router DHCP");
    while (true) delay(1000);
  }

  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setAudioStreamPort(AUDIO_STREAM_PORT);
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
  if (store.kind() != ESP32P4_STORAGE_SD && sd.mounted()) {
    static WfmStorageFS sdVol(
        sd.fs(), "SD", []() -> uint64_t { return sd.totalBytes(); },
        []() -> uint64_t { return sd.usedBytes(); });
    sdVol.begin();
    wfm->addVolume("SD", sdVol);
    Serial.println("WFM: added secondary volume SD");
  }

  IPAddress ip = ETH.localIP();
  Serial.printf("Camera UI  http://%s/\n", ip.toString().c_str());
  Serial.printf("Video MJPEG http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.printf("Live audio http://%s:%u/audio.pcm (UI Play live audio)\n", ip.toString().c_str(),
                (unsigned)AUDIO_STREAM_PORT);
  Serial.printf("Files      http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.println("Capture -> /IMG/*.jpg | Record -> /VIDEO/*.mp4 with mic audio");
}

void loop() {
  stream.loop();
  if (wfm && !stream.isRecording()) wfm->loop();

  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("[hb] sent=%u rec=%u videos=%u mic=%s rms=%.2f\n", (unsigned)stream.sent(),
                  stream.isRecording() ? 1u : 0u, (unsigned)stream.videosSaved(),
                  mic.ready() ? "on" : "off", mic.ready() ? mic.rms() : 0.0f);
  }
}
