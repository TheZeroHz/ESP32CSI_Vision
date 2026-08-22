/**
 * 32_EthCocoWeb — Ethernet MJPEG + ESP-DL object detect zoo
 *
 * Vendored under src/espdl/ (COCO YOLO11n, YOLO26n, pedestrian, cat, dog, hand).
 *
 * Storage preference (edit APP_STORAGE below):
 *   ESP32P4_STORAGE_AUTO      SD → FFat → LittleFS → SPIFFS
 *   ESP32P4_STORAGE_SD        microSD only
 *
 * On the chosen volume:
 *   /models/p4/coco_detect_yolo11n_s8_v1.espdl
 *   /models/p4/coco_detect_yolo11n_320_s8_v1.espdl      (optional)
 *   /models/p4/yolo26n_640_s8_p4.espdl                  (optional)
 *   /models/p4/yolo26n_512_s8_p4.espdl                  (optional)
 *   /models/p4/pedestrian_detect_pico_s8_v1.espdl       (optional)
 *   /models/p4/espdet_pico_*_{cat,dog,hand}.espdl       (optional)
 *   /det/settings.txt
 *
 * Copy from library models/espdl/p4/ or upload via WebFileManager :82.
 *
 * Ports: :80 Camera/Detect UI · :82 WebFileManager · :83 WFM transfers
 * Wiring: board_config.h  (docs/Custom-Boards.md)
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
#define APP_NAME "32_EthCocoWeb"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

static const uint16_t CAM_UI_PORT = 80;
static const uint16_t WFM_UI_PORT = 82;
static const uint16_t WFM_FILE_PORT = 83;

static const char *kSettingsFs = "/det/settings.txt";

static char kSettings[64];

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_ObjectDetect det;

WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

static volatile bool eth_ready = false;
static bool det_ready = false;
static esp32p4_det_t g_dets[16];

static void rebuildVfsPaths() {
  store.vfsPath(kSettings, sizeof(kSettings), "/det/settings.txt");
}

static int modelFromUi(int m) {
  switch (m) {
    case 1: return ESP32P4_DET_COCO_YOLO11N_320;
    case 2: return ESP32P4_DET_PEDESTRIAN_PICO;
    case 3: return ESP32P4_DET_CAT_224;
    case 4: return ESP32P4_DET_CAT_416;
    case 5: return ESP32P4_DET_DOG_224;
    case 6: return ESP32P4_DET_DOG_416;
    case 7: return ESP32P4_DET_HAND_224;
    case 8: return ESP32P4_DET_YOLO26_640;
    case 9: return ESP32P4_DET_YOLO26_512;
    default: return ESP32P4_DET_COCO_YOLO11N;
  }
}

static const char *modelFile(int m) {
  switch (m) {
    case 1: return "/models/p4/coco_detect_yolo11n_320_s8_v1.espdl";
    case 2: return "/models/p4/pedestrian_detect_pico_s8_v1.espdl";
    case 3: return "/models/p4/espdet_pico_224_224_cat.espdl";
    case 4: return "/models/p4/espdet_pico_416_416_cat.espdl";
    case 5: return "/models/p4/espdet_pico_224_224_dog.espdl";
    case 6: return "/models/p4/espdet_pico_416_416_dog.espdl";
    case 7: return "/models/p4/espdet_pico_224_224_hand.espdl";
    case 8: return "/models/p4/yolo26n_640_s8_p4.espdl";
    case 9: return "/models/p4/yolo26n_512_s8_p4.espdl";
    default: return "/models/p4/coco_detect_yolo11n_s8_v1.espdl";
  }
}

static bool modelsPresent(int model = -1) {
  int m = model;
  if (m < 0) m = stream.detUi().model;
  return store.exists(modelFile(m));
}

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
    else if (sscanf(line, "model=%d", &v) == 1) ui.model = v;
    else if (sscanf(line, "thr=%d", &v) == 1) ui.thr_pct = v;
    else if (sscanf(line, "quality=%d", &v) == 1) stream.setQuality((uint8_t)v);
    else if (sscanf(line, "framesize=%d", &v) == 1) stream.setFramesize((uint8_t)v);
  }
  fclose(f);
  if (ui.thr_pct < 5) ui.thr_pct = 5;
  if (ui.thr_pct > 95) ui.thr_pct = 95;
  if (ui.model < 0 || ui.model > 9) ui.model = 0;
}

static void fillSummary(const esp32p4_det_t *dets, int n) {
  auto &ui = stream.detUi();
  ui.summary[0] = '\0';
  size_t o = 0;
  for (int i = 0; i < n && o + 16 < sizeof(ui.summary); i++) {
    const char *lab = det.label(dets[i].category);
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

  if (ui.model_req) {
    ui.model_req = false;
    det.end();
    det_ready = false;
    if (modelsPresent(ui.model) && det.begin(modelFromUi(ui.model))) {
      det.setScoreThr((float)ui.thr_pct / 100.f);
      det_ready = true;
    } else {
      Serial.printf("det: model %d missing on %s\n", ui.model, store.label());
    }
  }
  if (ui.thr_req) {
    ui.thr_req = false;
    if (det.ready()) det.setScoreThr((float)ui.thr_pct / 100.f);
  }
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }

  if (!ui.detect_en || !det.ready()) {
    ui.objs = 0;
    ui.ms = 0;
    ui.summary[0] = '\0';
    return;
  }

  int n = det.detect(rgb, w, h, g_dets, 16);
  ui.objs = n;
  ui.ms = det.lastMs();
  fillSummary(g_dets, n);
  if (n > 0) {
    det.draw(rgb, w, h, g_dets, n, det.model());
  }
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-coco");
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
  wfm->setName("COCO models")
      .setPorts(WFM_UI_PORT, WFM_FILE_PORT)
      .setHomePort(CAM_UI_PORT);
  if (!wfm->begin()) {
    Serial.println("WebFileManager begin FAILED");
    return false;
  }
  wfm->startFileTask();
  return true;
}

static void printUploadHelp(const IPAddress &ip) {
  Serial.println();
  Serial.printf("Missing /models/p4/*.espdl on %s — upload via WebFileManager:\n", store.label());
  Serial.printf("  Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("  Put under /%s/models/p4/  (see library models/espdl/p4/)\n", store.label());
  Serial.println("  Detect models: coco / yolo26 / pedestrian / cat / dog / hand …");
  Serial.println("  Also available: pose, seg, imagenet, gesture, ocr, reid, speaker");
  Serial.println("Waiting for uploads...");
}

static bool startCameraDet() {
  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    return false;
  }

  auto &ui = stream.detUi();
  if (!det.begin(modelFromUi(ui.model))) {
    Serial.printf("ObjectDetect.begin FAILED — camera UI still up, upload via Files :%u\n",
                  (unsigned)WFM_UI_PORT);
    stream.setModelMissingNote(store.volumeSummary());
    snprintf(ui.summary, sizeof(ui.summary), "need model · Files :%u", WFM_UI_PORT);
  } else {
    det.setScoreThr((float)ui.thr_pct / 100.f);
    det_ready = true;
  }

  stream.begin(&cam, CAM_UI_PORT, 38);
  stream.setFramesize(ESP32P4_STREAM_HD);
  stream.enableDetUi(true);
  static const int kDetVals[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  static const char *kDetLabs[] = {
      "YOLO11n · 640 · COCO", "YOLO11n · 320 · COCO", "Pico · pedestrian",
      "ESPDet · cat 224",     "ESPDet · cat 416",     "ESPDet · dog 224",
      "ESPDet · dog 416",     "ESPDet · hand 224",    "YOLO26n · 640 · COCO",
      "YOLO26n · 512 · COCO"};
  stream.setDetCatalog("Detect", "Object detect",
                       "COCO / YOLO26 / cat / dog / hand / pedestrian · /models/p4/", 10, kDetVals,
                       kDetLabs);
  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setFrameHook(detHook, nullptr);

  loadSettings();
  if (det_ready) {
    if (ui.model != (int)det.model()) {
      det.end();
      det_ready = false;
      if (modelsPresent(ui.model) && det.begin(modelFromUi(ui.model))) {
        det.setScoreThr((float)ui.thr_pct / 100.f);
        det_ready = true;
        stream.setPreviewNote("");
      }
    } else {
      det.setScoreThr((float)ui.thr_pct / 100.f);
    }
  }
  saveSettings();

  IPAddress ip = ETH.localIP();
  Serial.printf("UI     http://%s/\n", ip.toString().c_str());
  Serial.printf("Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("Storage %s vfs=%s\n", store.label(), store.vfsRoot());
  Serial.printf("Detect ready=%d model=%d thr=%d%%\n", det.ready() ? 1 : 0, (int)det.model(),
                ui.thr_pct);
  return true;
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 32_EthCocoWeb (ESP-DL COCO / YOLO26 / cat / dog / hand) ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("APP_STORAGE pref=%s\n", store.kindName(APP_STORAGE));

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage mount FAILED (SD / FFat / LittleFS / SPIFFS)");
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
    if (det_ready) {
      auto &ui = stream.detUi();
      Serial.printf("[hb] objs=%d ms=%d det=%d model=%d thr=%d summary=%s\n", ui.objs, ui.ms,
                    ui.detect_en ? 1 : 0, ui.model, ui.thr_pct, ui.summary[0] ? ui.summary : "-");
    } else {
      Serial.println("[hb] waiting for models via WebFileManager");
    }
  }
}
