/**
 * 39_EthGestureWeb — Ethernet MJPEG + ESP-DL hand detect + 8-gesture classify
 *
 * Models on the chosen volume:
 *   /models/p4/espdet_pico_224_224_hand.espdl
 *   /models/p4/mobilenetv2_0_5_128_128_gesture.espdl
 *
 * Copy from library models/espdl/p4/ or upload via WebFileManager :82.
 * Ports: :80 Camera/Gesture UI · :82 WebFileManager · :83 WFM transfers
 * Board: Guition JC-ESP32P4-M3
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <Arduino.h>
#include <FS.h>
#include <stdio.h>
#include <string.h>


#include "board_config.h"
#include <ETH.h>

#ifndef APP_NAME
#define APP_NAME "39_EthGestureWeb"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

static const uint16_t CAM_UI_PORT = 80;
static const uint16_t WFM_UI_PORT = 82;
static const uint16_t WFM_FILE_PORT = 83;
static const char *kSettingsFs = "/det/settings.txt";
static char kSettings[64];
static const int kVals[] = {0};
static const char *kLabs[] = {"Hand + gesture"};
static const int kN = 1;
static const char *kHand = "/models/p4/espdet_pico_224_224_hand.espdl";
static const char *kCls = "/models/p4/mobilenetv2_0_5_128_128_gesture.espdl";

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_Gesture gest;
WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

static volatile bool eth_ready = false;
static bool det_ready = false;
static esp32p4_gesture_t g_g[8];

static void rebuildVfsPaths() { store.vfsPath(kSettings, sizeof(kSettings), kSettingsFs); }
static bool modelsPresent() { return store.exists(kHand) && store.exists(kCls); }

static void ensureModelDirs() {
  if (!store.exists("/models")) store.mkdir("/models");
  if (!store.exists("/models/p4")) store.mkdir("/models/p4");
  if (!store.exists("/det")) store.mkdir("/det");
}

static void saveSettings() {
  auto &ui = stream.detUi();
  ensureModelDirs();
  char buf[128];
  snprintf(buf, sizeof(buf), "detect=%d\nmodel=%d\nthr=%d\nquality=%u\nframesize=%u\n",
           ui.detect_en ? 1 : 0, ui.model, ui.thr_pct, (unsigned)stream.quality(),
           (unsigned)stream.framesize());
  File f = store.fs().open(kSettingsFs, FILE_WRITE);
  if (f) {
    f.print(buf);
    f.flush();
    f.close();
  }
}

static void loadSettings() {
  auto &ui = stream.detUi();
  ui.detect_en = true;
  ui.model = 0;
  ui.thr_pct = 25;
  FILE *f = fopen(kSettings, "r");
  if (!f) return;
  char line[80];
  while (fgets(line, sizeof(line), f)) {
    int v = 0;
    if (sscanf(line, "detect=%d", &v) == 1) ui.detect_en = v != 0;
    else if (sscanf(line, "thr=%d", &v) == 1) ui.thr_pct = v;
    else if (sscanf(line, "quality=%d", &v) == 1) stream.setQuality((uint8_t)v);
    else if (sscanf(line, "framesize=%d", &v) == 1) stream.setFramesize((uint8_t)v);
  }
  fclose(f);
  if (ui.thr_pct < 5) ui.thr_pct = 5;
  if (ui.thr_pct > 95) ui.thr_pct = 95;
  ui.model = 0;
}

static void fillSummary(const esp32p4_gesture_t *g, int n) {
  auto &ui = stream.detUi();
  ui.summary[0] = '\0';
  size_t o = 0;
  for (int i = 0; i < n && o + 16 < sizeof(ui.summary); i++) {
    const char *lab = g[i].label ? g[i].label : "?";
    if (i) {
      ui.summary[o++] = ' ';
      ui.summary[o] = '\0';
    }
    size_t len = strlen(lab);
    if (o + len >= sizeof(ui.summary)) break;
    memcpy(ui.summary + o, lab, len);
    o += len;
    ui.summary[o] = '\0';
  }
}

static void detHook(uint16_t *rgb, int w, int h, void *) {
  if (!det_ready) return;
  auto &ui = stream.detUi();
  ui.model_req = false;
  if (ui.thr_req) {
    ui.thr_req = false;
    if (gest.ready()) gest.setScoreThr((float)ui.thr_pct / 100.f);
  }
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }
  if (!ui.detect_en || !gest.ready()) {
    ui.objs = 0;
    ui.ms = 0;
    ui.summary[0] = '\0';
    return;
  }
  int n = gest.detect(rgb, w, h, g_g, 8);
  ui.objs = n;
  ui.ms = gest.lastMs();
  fillSummary(g_g, n);
  if (n > 0) gest.draw(rgb, w, h, g_g, n);
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-gesture");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      eth_ready = true;
      Serial.println(ETH);
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

static bool startEthAndWfm() {
  Network.onEvent(onEthEvent);
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE)) {
    Serial.println("ETH FAILED");
    return false;
  }
  uint32_t t0 = millis();
  while (!eth_ready && millis() - t0 < 30000) delay(200);
  if (!eth_ready) {
    Serial.println("no DHCP");
    return false;
  }
  if (!wfm) return false;
  wfm->setName("Gesture models").setPorts(WFM_UI_PORT, WFM_FILE_PORT).setHomePort(CAM_UI_PORT);
  if (!wfm->begin()) {
    Serial.println("WebFileManager begin FAILED");
    return false;
  }
  wfm->startFileTask();
  return true;
}

static void printUploadHelp(const IPAddress &ip) {
  Serial.println();
  Serial.printf("Missing gesture .espdl on %s — upload via WebFileManager:\n", store.label());
  Serial.printf("  Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("  Put under /%s/models/p4/  (hand + mobilenetv2 gesture)\n", store.label());
  Serial.println("Waiting for uploads...");
}

static bool startCameraDet() {
  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    return false;
  }
  auto &ui = stream.detUi();
  if (!gest.begin()) {
    Serial.printf("Gesture begin FAILED — camera UI still up, upload via Files :%u\n",
                  (unsigned)WFM_UI_PORT);
    stream.setModelMissingNote(store.volumeSummary());
    snprintf(ui.summary, sizeof(ui.summary), "need model · Files :%u", WFM_UI_PORT);
  } else {
    gest.setScoreThr((float)ui.thr_pct / 100.f);
    det_ready = true;
  }
  stream.begin(&cam, CAM_UI_PORT, 38);
  stream.setFramesize(ESP32P4_STREAM_HD);
  stream.enableDetUi(true);
  stream.setDetCatalog("Gesture", "Hand gesture", "Pico hand + MobileNetV2 8-class · /models/p4/", kN,
                       kVals, kLabs);
  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setFrameHook(detHook, nullptr);
  loadSettings();
  if (det_ready) gest.setScoreThr((float)ui.thr_pct / 100.f);
  saveSettings();
  IPAddress ip = ETH.localIP();
  Serial.printf("UI     http://%s/\n", ip.toString().c_str());
  Serial.printf("Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("Gesture ready=%d thr=%d%%\n", gest.ready() ? 1 : 0, ui.thr_pct);
  return true;
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 39_EthGestureWeb (ESP-DL hand gesture) ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage mount FAILED");
    while (true) delay(1000);
  }
  rebuildVfsPaths();
  static WfmStorageFS primaryVol(
      store.fs(), store.label(), []() -> uint64_t { return store.totalBytes(); },
      []() -> uint64_t { return store.usedBytes(); });
  static WebFileManager wfmMgr(primaryVol);
  primaryVol.begin();
  appVol = &primaryVol;
  wfm = &wfmMgr;
  store.attachToWfm(*wfm);
  ensureModelDirs();
  if (!startEthAndWfm()) {
    while (true) delay(1000);
  }
  IPAddress ip = ETH.localIP();
  loadSettings();
  if (!startCameraDet()) {
    while (true) {
      wfm->loop();
      delay(2);
    }
  }
}

void loop() {
  stream.loop();
  if (wfm) wfm->loop();
  if (det_ready) {
    auto &ui = stream.detUi();
    if (ui.settings_dirty) {
      ui.settings_dirty = false;
      saveSettings();
    }
  }
  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    auto &ui = stream.detUi();
    Serial.printf("[hb] hands=%d ms=%d det=%d %s\n", ui.objs, ui.ms, ui.detect_en ? 1 : 0,
                  ui.summary[0] ? ui.summary : "-");
  }
}
