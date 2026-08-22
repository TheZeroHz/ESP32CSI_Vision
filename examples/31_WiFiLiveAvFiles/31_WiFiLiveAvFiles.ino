/**
 * 31_WiFiLiveAvFiles — Wi-Fi live video + browser audio + FileMgr
 *
 * Camera / SD / mic / C6 Wi-Fi: board_config.h (docs/Custom-Boards.md).
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 4096
#endif

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#ifndef APP_NAME
#define APP_NAME "31_WiFiLiveAvFiles"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include "board_config.h"

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
esp32csi_wifi_config_t wifi_cfg = esp32csi_wifi_config();

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_H264 h264;
ESP32P4_Mic mic;
ESP32P4_MjpegServer stream;

WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 31_WiFiLiveAvFiles ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("board=%s storage=%s\n", esp32csi_board_name(),
                store.kindName(APP_STORAGE));

  esp32p4_prefer_psram();
  cam_cfg.fb_count = 2;
  cam_cfg.frame_size = ESP32P4_FRAMESIZE_HD;
  wifi_cfg.hostname = "esp32p4-live-av";
  esp32csi_print_cam_config(cam_cfg);
  esp32csi_print_sd_config(sd_cfg);
  esp32csi_print_wifi_config(wifi_cfg);

  // Override camera pins or settings here when using a custom carrier.
  // cam_cfg.sda = 7; cam_cfg.scl = 8;
  // cam_cfg.sensor = ESP32P4_SENSOR_OV5647;

  // Override SD pins here for non-Guition wiring.
  // sd_cfg.clk = 43; sd_cfg.cmd = 44;
  // sd_cfg.d0 = 39; sd_cfg.d1 = 40; sd_cfg.d2 = 41; sd_cfg.d3 = 42;

  // Override mic pins / type here when your ES8311 wiring differs.
  // mic_cfg.type = ESP32P4_MIC_ES8311;
  // mic_cfg.i2c_sda = 7; mic_cfg.i2c_scl = 8;
  // mic_cfg.wire = &Wire1;  // I2C1 instead of Wire (I2C0)
  // mic_cfg.i2s_mclk = 13; mic_cfg.i2s_bclk = 12; mic_cfg.i2s_ws = 10;
  // mic_cfg.i2s_dout = 9; mic_cfg.i2s_din = 48; mic_cfg.pa_gpio = 11;
  mic_cfg.sample_rate = 16000;

  // Override C6 SDIO Wi-Fi pins here if your carrier differs from Guition M3.
  // wifi_cfg.clk = 18; wifi_cfg.cmd = 19;
  // wifi_cfg.d0 = 14; wifi_cfg.d1 = 15; wifi_cfg.d2 = 16; wifi_cfg.d3 = 17;
  // wifi_cfg.rst = 54;

  if (!cam.begin(cam_cfg)) {
    Serial.println("camera FAILED - check OV5647 CSI module");
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

  if (!esp32csi_wifi_begin(wifi_cfg)) {
    Serial.println("Wi-Fi FAILED");
    while (true) delay(1000);
  }

  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setAudioStreamPort(AUDIO_STREAM_PORT);
  if (!stream.begin(&cam, CAM_UI_PORT, 25)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }
  // C6 SDIO Wi-Fi: XGA + smaller JPEG (q=25, 4:2:0). Skip=0 keeps it live.
  (void)stream.setFramesize(ESP32P4_STREAM_XGA);
  stream.setFrameSkip(0);

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

  IPAddress ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
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
  const bool rec = stream.isRecording() || stream.isFinalizing();
  if (wfm) {
    wfm->setPaused(rec);
    wfm->loop();
    if (!rec) stream.setEncodePaused(wfm->isBusy());
  }

  if (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED) {
    static uint32_t last_re = 0;
    if (millis() - last_re > 5000) {
      last_re = millis();
      Serial.println("WiFi lost - reconnect");
      CSI_STALL(ESP32P4_DBG_WIFI, "STA lost - reconnect");
      WiFi.reconnect();
    }
  }

  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    static const char *kPhase[] = {"idle", "cap", "ppa", "jpg", "slot"};
    uint8_t ph = stream.workerPhase();
    if (ph > 4) ph = 0;
    Serial.printf(
        "[hb] sent=%u drop=%u jpeg=%u enc=%ums phase=%s age=%ums capf=%u jpgf=%u "
        "csi=%u/%u busy=%u ppa_to=%u wifi=%d rssi=%d heap=%u dma=%u rec=%u\n",
        (unsigned)stream.sent(), (unsigned)stream.dropped(),
        (unsigned)stream.lastJpegBytes(), (unsigned)stream.encodeMs(), kPhase[ph],
        (unsigned)stream.lastFrameAgeMs(), (unsigned)stream.captureFails(),
        (unsigned)stream.jpegFails(), (unsigned)cam.doneCount(), (unsigned)cam.dropCount(),
        (unsigned)stream.jpgBusyMask(), (unsigned)stream.ppaTimeouts(), (int)WiFi.status(),
        (int)WiFi.RSSI(),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
        stream.isRecording() ? 1u : 0u);
  }
}
