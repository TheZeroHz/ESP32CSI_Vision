/**
 * 41_EthOcrWeb — Ethernet MJPEG + ESP-DL PaddleOCR v6
 *
 * Models on the chosen volume:
 *   /models/p4/pp_ocr_v6_det_s8.espdl
 *   /models/p4/pp_ocr_v6_rec_s16.espdl     (default)
 *   /models/p4/pp_ocr_v6_rec_s8.espdl      (optional)
 *
 * Copy from library models/espdl/p4/ or upload via WebFileManager :82.
 * Ports: :80 Camera/OCR UI · :82 WebFileManager · :83 WFM transfers
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
#define APP_NAME "41_EthOcrWeb"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

static const uint16_t CAM_UI_PORT = 80;
static const uint16_t WFM_UI_PORT = 82;
static const uint16_t WFM_FILE_PORT = 83;
static const char *kSettingsFs = "/det/settings.txt";
static char kSettings[64];

static const int kVals[] = {(int)ESP32P4_OCR_REC_S16, (int)ESP32P4_OCR_REC_S8};
static const char *kLabs[] = {"PP-OCR rec s16", "PP-OCR rec s8"};
static const int kN = 2;
static const char *kDet = "/models/p4/pp_ocr_v6_det_s8.espdl";
static const char *kRec16 = "/models/p4/pp_ocr_v6_rec_s16.espdl";
static const char *kRec8 = "/models/p4/pp_ocr_v6_rec_s8.espdl";

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_Ocr ocr;
WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

static volatile bool eth_ready = false;
static bool det_ready = false;
static esp32p4_ocr_t g_ocr[8];

static void rebuildVfsPaths() { store.vfsPath(kSettings, sizeof(kSettings), kSettingsFs); }

static int recFromUi(int m) {
  return (m == (int)ESP32P4_OCR_REC_S8) ? ESP32P4_OCR_REC_S8 : ESP32P4_OCR_REC_S16;
}

static const char *recFile(int m) {
  return (m == (int)ESP32P4_OCR_REC_S8) ? kRec8 : kRec16;
}

static bool modelsPresent(int model = -1) {
  int m = model;
  if (m < 0) m = stream.detUi().model;
  return store.exists(kDet) && store.exists(recFile(m));
}

static void ensureModelDirs() {
  if (!store.exists("/models")) store.mkdir("/models");
  if (!store.exists("/models/p4")) store.mkdir("/models/p4");
  if (!store.exists("/det")) store.mkdir("/det");
}

static void clampModel() {
  auto &ui = stream.detUi();
  for (int i = 0; i < kN; i++) {
    if (ui.model == kVals[i]) return;
  }
  ui.model = kVals[0];
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
  ui.model = kVals[0];
  ui.thr_pct = 50;
  FILE *f = fopen(kSettings, "r");
  if (!f) return;
  char line[80];
  while (fgets(line, sizeof(line), f)) {
    int v = 0;
    if (sscanf(line, "detect=%d", &v) == 1) ui.detect_en = v != 0;
    else if (sscanf(line, "model=%d", &v) == 1) ui.model = v;
    else if (sscanf(line, "thr=%d", &v) == 1) ui.thr_pct = v;
    else if (sscanf(line, "quality=%d", &v) == 1) stream.setQuality((uint8_t)v);
    else if (sscanf(line, "framesize=%d", &v) == 1) stream.setFramesize((uint8_t)v);
  }
  fclose(f);
  if (ui.thr_pct < 5) ui.thr_pct = 5;
  if (ui.thr_pct > 95) ui.thr_pct = 95;
  clampModel();
}

static void fillSummary(const esp32p4_ocr_t *hits, int n) {
  auto &ui = stream.detUi();
  ui.summary[0] = '\0';
  size_t o = 0;
  for (int i = 0; i < n && o + 8 < sizeof(ui.summary); i++) {
    if (i) {
      ui.summary[o++] = ' ';
      ui.summary[o] = '\0';
    }
    size_t len = strlen(hits[i].text);
    if (o + len >= sizeof(ui.summary)) break;
    memcpy(ui.summary + o, hits[i].text, len);
    o += len;
    ui.summary[o] = '\0';
  }
}

static bool beginOcr(int model, int thr_pct) {
  ocr.end();
  if (!modelsPresent(model)) return false;
  if (!ocr.begin(recFromUi(model), ESP32P4_OCR_SHORT)) return false;
  ocr.setScoreThr((float)thr_pct / 100.f);
  return true;
}

static void detHook(uint16_t *rgb, int w, int h, void *) {
  if (!det_ready) return;
  auto &ui = stream.detUi();
  if (ui.model_req) {
    ui.model_req = false;
    det_ready = false;
    if (beginOcr(ui.model, ui.thr_pct)) det_ready = true;
    else Serial.printf("ocr: rec model %d missing on %s\n", ui.model, store.label());
  }
  if (ui.thr_req) {
    ui.thr_req = false;
    if (ocr.ready()) ocr.setScoreThr((float)ui.thr_pct / 100.f);
  }
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }
  if (!ui.detect_en || !ocr.ready()) {
    ui.objs = 0;
    ui.ms = 0;
    ui.summary[0] = '\0';
    return;
  }
  int n = ocr.run(rgb, w, h, g_ocr, 8);
  ui.objs = n;
  ui.ms = ocr.lastMs();
  fillSummary(g_ocr, n);
  if (n > 0) ocr.draw(rgb, w, h, g_ocr, n);
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-ocr");
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
  wfm->setName("OCR models").setPorts(WFM_UI_PORT, WFM_FILE_PORT).setHomePort(CAM_UI_PORT);
  if (!wfm->begin()) {
    Serial.println("WebFileManager begin FAILED");
    return false;
  }
  wfm->startFileTask();
  return true;
}

static void printUploadHelp(const IPAddress &ip) {
  Serial.println();
  Serial.printf("Missing OCR .espdl on %s — upload via WebFileManager:\n", store.label());
  Serial.printf("  Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("  Put under /%s/models/p4/  (pp_ocr_v6_det_s8 + rec_s16)\n", store.label());
  Serial.println("Waiting for uploads...");
}

static bool startCameraDet() {
  auto &ui = stream.detUi();
  // Load OCR *before* CSI framebuffers so the ~2.4 MB weight blob gets a
  // contiguous PSRAM block. 1080p x3 FBs leave only ~1.5 MB largest block.
  if (modelsPresent(ui.model)) {
    det_ready = beginOcr(ui.model, ui.thr_pct);
    if (!det_ready) {
      Serial.println("OCR begin FAILED (will keep camera UI up)");
    }
  } else {
    Serial.printf("OCR models missing on %s — camera UI still up, upload via Files :%u\n",
                  store.label(), (unsigned)WFM_UI_PORT);
  }

  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  cfg.fb_count = 2;
  cfg.frame_size = ESP32P4_FRAMESIZE_HD;
  if (!cam.begin(cfg)) {
    Serial.println("camera FAILED");
    return false;
  }
  if (!det_ready) {
    stream.setModelMissingNote(store.volumeSummary());
    snprintf(ui.summary, sizeof(ui.summary), "need model · Files :%u", WFM_UI_PORT);
  }
  stream.begin(&cam, CAM_UI_PORT, 38);
  stream.setFramesize(ESP32P4_STREAM_HD);
  stream.enableDetUi(true);
  stream.setDetCatalog("OCR", "PaddleOCR v6", "Detect + rec s16/s8 · /models/p4/", kN, kVals, kLabs);
  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setFrameHook(detHook, nullptr);
  loadSettings();
  if (ocr.ready()) ocr.setScoreThr((float)ui.thr_pct / 100.f);
  saveSettings();
  IPAddress ip = ETH.localIP();
  Serial.printf("UI     http://%s/\n", ip.toString().c_str());
  Serial.printf("Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("OCR ready=%d rec=%d thr=%d%%\n", ocr.ready() ? 1 : 0, ui.model, ui.thr_pct);
  return true;
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 41_EthOcrWeb (ESP-DL PaddleOCR v6) ===");
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
    Serial.printf("[hb] lines=%d ms=%d det=%d %s\n", ui.objs, ui.ms, ui.detect_en ? 1 : 0,
                  ui.summary[0] ? ui.summary : "-");
  }
}
