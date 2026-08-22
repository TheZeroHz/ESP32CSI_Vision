/**
 * 33_EthYolo26Web — Ethernet MJPEG + ESP-DL YOLO26n (COCO-80)
 *
 * Models on the chosen volume:
 *   /models/p4/yolo26n_640_s8_p4.espdl
 *   /models/p4/yolo26n_512_s8_p4.espdl   (optional)
 *
 * Copy from library models/espdl/p4/ or upload via WebFileManager :82.
 * Ports: :80 Camera/Detect UI · :82 WebFileManager · :83 WFM transfers
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
#include <esp_heap_caps.h>

#ifndef APP_NAME
#define APP_NAME "33_EthYolo26Web"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif

static const uint16_t CAM_UI_PORT = 80;
static const uint16_t WFM_UI_PORT = 82;
static const uint16_t WFM_FILE_PORT = 83;
static const char *kSettingsFs = "/det/settings.txt";
static char kSettings[64];

static const int kVals[] = {(int)ESP32P4_DET_YOLO26_512,
                            (int)ESP32P4_DET_YOLO26_640};
static const char *kLabs[] = {"YOLO26n · 512 · COCO (~2s)", "YOLO26n · 640 · COCO (~3.5s)"};
static const int kN = 2;

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_ObjectDetect det;
ESP32P4_Img img;
WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

static volatile bool eth_ready = false;
static bool stream_ok = false;
static bool det_ready = false;
static esp32p4_det_t g_dets[16];
static int g_n = 0;
static uint32_t g_last_det_ms = 0;
static uint32_t g_det_result_ms = 0;
static uint16_t *g_det_rgb = nullptr;
static int g_det_w = 0;
static int g_det_h = 0;
static SemaphoreHandle_t g_det_job = nullptr;
static volatile bool g_det_busy = false;

static void fillSummary(const esp32p4_det_t *dets, int n);

static uint8_t meanLuma565(const uint16_t *p, int n) {
  if (!p || n <= 0) return 0;
  uint32_t s = 0;
  int step = n / 256;
  if (step < 1) step = 1;
  int c = 0;
  for (int i = 0; i < n; i += step) {
    s += img.luma565(p[i]);
    c++;
  }
  return (uint8_t)(s / (uint32_t)(c ? c : 1));
}

static void detTask(void *) {
  Serial.println("[yolo] task start");
  for (;;) {
    if (!g_det_job || xSemaphoreTake(g_det_job, portMAX_DELAY) != pdTRUE) continue;
    Serial.println("[yolo] wake");
    auto &ui = stream.detUi();
    if (ui.detect_en && det.ready() && g_det_rgb && g_det_w > 0) {
      uint8_t luma = meanLuma565(g_det_rgb, g_det_w * g_det_h);
      if (luma < 18) {
        g_n = 0;
        ui.objs = 0;
        ui.ms = 0;
        g_det_result_ms = 0;
        snprintf(ui.summary, sizeof(ui.summary), "too dark (luma %u)", luma);
        Serial.printf("[yolo] skip dark luma=%u\n", luma);
      } else {
        disableCore0WDT();
        esp32p4_psram_msync(g_det_rgb, (size_t)g_det_w * (size_t)g_det_h * 2u);
        g_n = det.detect(g_det_rgb, g_det_w, g_det_h, g_dets, 16);
        enableCore0WDT();
        ui.objs = g_n;
        ui.ms = det.lastMs();
        fillSummary(g_dets, g_n);
        g_det_result_ms = millis();
        Serial.printf("[yolo] n=%d ms=%d", g_n, ui.ms);
        for (int i = 0; i < g_n && i < 4; i++) {
          Serial.printf(" %s:%.2f", det.label(g_dets[i].category), g_dets[i].score);
        }
        Serial.printf(" %dx%d luma=%u\n", g_det_w, g_det_h, luma);
      }
    } else {
      Serial.printf("[yolo] skip en=%d ready=%d rgb=%d wh=%dx%d\n", ui.detect_en ? 1 : 0,
                    det.ready() ? 1 : 0, g_det_rgb ? 1 : 0, g_det_w, g_det_h);
    }
    g_det_busy = false;
  }
}

static void rebuildVfsPaths() { store.vfsPath(kSettings, sizeof(kSettings), kSettingsFs); }

static int modelFromUi(int m) {
  return (m == (int)ESP32P4_DET_YOLO26_512) ? ESP32P4_DET_YOLO26_512
                                                     : ESP32P4_DET_YOLO26_640;
}

static const char *modelFile(int m) {
  return (m == (int)ESP32P4_DET_YOLO26_512) ? "/models/p4/yolo26n_512_s8_p4.espdl"
                                                     : "/models/p4/yolo26n_640_s8_p4.espdl";
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
  clampModel();
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
  auto &ui = stream.detUi();
  if (ui.model_req) {
    ui.model_req = false;
    while (g_det_busy) delay(10);
    det.end();
    det_ready = false;
    g_n = 0;
    g_det_result_ms = 0;
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
    g_n = 0;
    return;
  }
  /* Drop stale overlays quickly so ghost boxes cannot ride along for seconds. */
  uint32_t hold_ms = (uint32_t)ui.ms / 3u + 250u;
  if (hold_ms < 450) hold_ms = 450;
  if (hold_ms > 1200) hold_ms = 1200;
  if (g_n > 0 && g_det_result_ms != 0 && (millis() - g_det_result_ms) > hold_ms) {
    g_n = 0;
    ui.objs = 0;
    ui.summary[0] = '\0';
  }
  if (g_n > 0) {
    esp32p4_det_t scaled[16];
    int n = 0;
    const int min_pct = ui.thr_pct;
    for (int i = 0; i < g_n && n < 16; i++) {
      int pct = (int)(g_dets[i].score * 100.f + 0.5f);
      if (pct < min_pct) continue;
      int x = g_dets[i].x, y = g_dets[i].y, bw = g_dets[i].w, bh = g_dets[i].h;
      if (g_det_w > 0 && g_det_h > 0 && (g_det_w != w || g_det_h != h)) {
        x = g_dets[i].x * w / g_det_w;
        y = g_dets[i].y * h / g_det_h;
        bw = g_dets[i].w * w / g_det_w;
        bh = g_dets[i].h * h / g_det_h;
      }
      int x2 = x + bw, y2 = y + bh;
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x2 > w) x2 = w;
      if (y2 > h) y2 = h;
      bw = x2 - x;
      bh = y2 - y;
      if (bw < 8 || bh < 8) continue;
      scaled[n] = g_dets[i];
      scaled[n].x = x;
      scaled[n].y = y;
      scaled[n].w = bw;
      scaled[n].h = bh;
      n++;
    }
    if (n > 0) det.draw(rgb, w, h, scaled, n, det.model());
  }
  uint32_t gap = 500;
  if (ui.ms > 0) gap = (uint32_t)ui.ms / 4u + 200u;
  if (gap < 350) gap = 350;
  if (gap > 900) gap = 900;
  if (!g_det_busy && g_det_rgb && g_det_job && millis() - g_last_det_ms >= gap) {
    int dw = w, dh = h;
    if (w >= 640 && h >= 360 && (w % 2) == 0 && (h % 2) == 0) {
      img.downsample2x565(rgb, w, h, g_det_rgb);
      dw = w / 2;
      dh = h / 2;
    } else if ((size_t)w * (size_t)h * 2u <= 640u * 360u * 2u) {
      memcpy(g_det_rgb, rgb, (size_t)w * (size_t)h * 2u);
    } else {
      return;
    }
    g_det_w = dw;
    g_det_h = dh;
    esp32p4_psram_writeback(g_det_rgb, (size_t)dw * (size_t)dh * 2u);
    g_last_det_ms = millis();
    g_det_busy = true;
    xSemaphoreGive(g_det_job);
    Serial.printf("[yolo] queue %dx%d ready=%d\n", dw, dh, det.ready() ? 1 : 0);
  }
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-yolo26");
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
  wfm->setName("YOLO26 models").setPorts(WFM_UI_PORT, WFM_FILE_PORT).setHomePort(CAM_UI_PORT);
  if (!wfm->begin()) {
    Serial.println("WebFileManager begin FAILED");
    return false;
  }
  wfm->startFileTask();
  return true;
}

static void printUploadHelp(const IPAddress &ip) {
  Serial.println();
  Serial.printf("Missing YOLO26 .espdl on %s — upload via WebFileManager:\n", store.label());
  Serial.printf("  Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("  Put under /%s/models/p4/  (yolo26n_640_s8_p4.espdl)\n", store.label());
  Serial.println("Waiting for uploads...");
}

static bool startCameraDet() {
  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  cfg.fb_count = 2; /* 3x 1080p RGB565 leaves no 4MB slab for YOLO26 tensors */
  if (!cam.begin(cfg)) {
    Serial.println("camera FAILED");
    return false;
  }
  /* IPA AE only. cam.setAEC(true) turns ISP AE off and lets OV5647 on-chip AGC hit 1023 (magenta grain). */
  if (cam.ispReady()) {
    cam.setAEC(false);
    cam.setAGC(false);
    cam.setIspAe(true);
  }
  cam.setGainCeiling(80);
  Serial.printf("PSRAM after cam %u  largest=%u\n", (unsigned)esp32p4_psram_free_size(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  /* Pause encode before worker starts so YOLO26 load isn't fighting PPA/JPEG DMA. */
  stream.setEncodePaused(true);
  if (!stream.begin(&cam, CAM_UI_PORT, 38)) {
    Serial.printf("stream.begin FAILED (PSRAM %u largest=%u)\n", (unsigned)esp32p4_psram_free_size(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    return false;
  }
  stream_ok = true;
  stream.setFramesize(ESP32P4_STREAM_HD);
  stream.enableDetUi(true);
  stream.setDetCatalog("YOLO26", "YOLO26n COCO", "Ultralytics YOLO26n · 80 classes · /models/p4/", kN,
                       kVals, kLabs);
  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setFrameHook(detHook, nullptr);
  Serial.printf("PSRAM after stream %u  largest=%u\n", (unsigned)esp32p4_psram_free_size(),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  loadSettings();
  auto &ui = stream.detUi();
  if (!det.begin(modelFromUi(ui.model))) {
    Serial.printf("YOLO26 begin FAILED — check models on %s (stream still live)\n", store.label());
    stream.setModelMissingNote(store.volumeSummary());
    snprintf(ui.summary, sizeof(ui.summary), "need model · Files :%u", WFM_UI_PORT);
  } else {
    det.setScoreThr((float)ui.thr_pct / 100.f);
    det_ready = true;
  }
  g_det_rgb = (uint16_t *)esp32p4_psram_alloc(640u * 360u * 2u);
  g_det_job = xSemaphoreCreateBinary();
  if (!g_det_rgb || !g_det_job ||
      xTaskCreatePinnedToCore(detTask, "yolo26", 32768, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.printf("YOLO26 async task FAILED (PSRAM %u largest=%u) — stream still live\n",
                  (unsigned)esp32p4_psram_free_size(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  }
  stream.setEncodePaused(false);
  if (!cam.ispReady()) stream.enableSmartAe(true);
  saveSettings();

  IPAddress ip = ETH.localIP();
  Serial.printf("UI     http://%s/\n", ip.toString().c_str());
  Serial.printf("Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("YOLO26 ready=%d model=%d thr=%d%% PSRAM=%u\n", det.ready() ? 1 : 0, (int)det.model(),
                ui.thr_pct, (unsigned)esp32p4_psram_free_size());
  return true;
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 33_EthYolo26Web (ESP-DL YOLO26n) ===");
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
  if (stream_ok) {
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
    Serial.printf("[hb] objs=%d ms=%d det=%d ready=%d busy=%d jpeg=%u %s\n", ui.objs, ui.ms,
                  ui.detect_en ? 1 : 0, det.ready() ? 1 : 0, g_det_busy ? 1 : 0,
                  (unsigned)stream.lastJpegBytes(), ui.summary[0] ? ui.summary : "-");
  }
}
