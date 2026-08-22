/**
 * 38_EthSegWeb — Ethernet MJPEG + ESP-DL COCO instance segmentation (YOLO11n-Seg)
 *
 * Models on the chosen volume:
 *   /models/p4/coco_seg_yolo11n_seg_s8_v1.espdl
 *
 * Copy from library models/espdl/p4/ or upload via WebFileManager :82.
 * Ports: :80 Camera/Seg UI · :82 WebFileManager · :83 WFM transfers
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
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

#ifndef APP_NAME
#define APP_NAME "38_EthSegWeb"
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
static const char *kLabs[] = {"YOLO11n-Seg"};
static const int kN = 1;
static const char *kModel = "/models/p4/coco_seg_yolo11n_seg_s8_v1.espdl";

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_Seg seg;
ESP32P4_Img img;
WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

static volatile bool eth_ready = false;
static bool det_ready = false;
static esp32p4_seg_t g_work[8];
static esp32p4_seg_t g_disp[8];
static int g_n = 0;
static uint32_t g_last_det_ms = 0;
static uint32_t g_det_result_ms = 0;
static uint16_t *g_det_rgb = nullptr;
static int g_det_w = 0;
static int g_det_h = 0;
static SemaphoreHandle_t g_det_job = nullptr;
static SemaphoreHandle_t g_res_mu = nullptr;
static volatile bool g_det_busy = false;
static uint8_t *g_disp_mask = nullptr;
static size_t g_disp_mask_cap = 0;

static void rebuildVfsPaths() { store.vfsPath(kSettings, sizeof(kSettings), kSettingsFs); }
static bool modelsPresent() { return store.exists(kModel); }

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

static void fillSummary(const esp32p4_seg_t *dets, int n) {
  auto &ui = stream.detUi();
  ui.summary[0] = '\0';
  size_t o = 0;
  for (int i = 0; i < n && o + 16 < sizeof(ui.summary); i++) {
    const char *lab = seg.label(dets[i].box.category);
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

static void snapSegs(const esp32p4_seg_t *src, int n) {
  size_t need = 0;
  for (int i = 0; i < n; i++) {
    if (src[i].mask && src[i].mask_w > 0 && src[i].mask_h > 0)
      need += (size_t)src[i].mask_w * (size_t)src[i].mask_h;
  }
  if (need > g_disp_mask_cap) {
    if (g_disp_mask) esp32p4_psram_free(g_disp_mask);
    g_disp_mask = (uint8_t *)esp32p4_psram_alloc(need ? need : 1);
    g_disp_mask_cap = g_disp_mask ? need : 0;
  }
  if (!g_res_mu || xSemaphoreTake(g_res_mu, pdMS_TO_TICKS(50)) != pdTRUE) return;
  size_t off = 0;
  g_n = 0;
  for (int i = 0; i < n && g_n < 8; i++) {
    g_disp[g_n] = src[i];
    if (src[i].mask && src[i].mask_w > 0 && src[i].mask_h > 0 && g_disp_mask &&
        off + (size_t)src[i].mask_w * (size_t)src[i].mask_h <= g_disp_mask_cap) {
      size_t m = (size_t)src[i].mask_w * (size_t)src[i].mask_h;
      memcpy(g_disp_mask + off, src[i].mask, m);
      g_disp[g_n].mask = g_disp_mask + off;
      off += m;
    } else {
      g_disp[g_n].mask = nullptr;
      g_disp[g_n].mask_w = 0;
      g_disp[g_n].mask_h = 0;
    }
    g_n++;
  }
  xSemaphoreGive(g_res_mu);
}

static void detTask(void *) {
  for (;;) {
    if (!g_det_job || xSemaphoreTake(g_det_job, portMAX_DELAY) != pdTRUE) continue;
    auto &ui = stream.detUi();
    if (ui.detect_en && seg.ready() && g_det_rgb && g_det_w > 0) {
      uint8_t luma = meanLuma565(g_det_rgb, g_det_w * g_det_h);
      if (luma < 18) {
        snapSegs(nullptr, 0);
        ui.objs = 0;
        ui.ms = 0;
        g_det_result_ms = 0;
        snprintf(ui.summary, sizeof(ui.summary), "too dark (luma %u)", luma);
      } else {
        disableCore0WDT();
        esp32p4_psram_msync(g_det_rgb, (size_t)g_det_w * (size_t)g_det_h * 2u);
        int n = seg.detect(g_det_rgb, g_det_w, g_det_h, g_work, 8);
        enableCore0WDT();
        snapSegs(g_work, n);
        ui.objs = n;
        ui.ms = seg.lastMs();
        fillSummary(g_work, n);
        g_det_result_ms = millis();
      }
    }
    g_det_busy = false;
  }
}

static void detHook(uint16_t *rgb, int w, int h, void *) {
  auto &ui = stream.detUi();
  ui.model_req = false;
  if (ui.thr_req) {
    ui.thr_req = false;
    if (seg.ready()) seg.setScoreThr((float)ui.thr_pct / 100.f);
  }
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }
  if (!ui.detect_en || !seg.ready()) {
    ui.objs = 0;
    ui.ms = 0;
    ui.summary[0] = '\0';
    if (g_res_mu && xSemaphoreTake(g_res_mu, 0) == pdTRUE) {
      g_n = 0;
      xSemaphoreGive(g_res_mu);
    }
    return;
  }
  if (g_res_mu && xSemaphoreTake(g_res_mu, 0) == pdTRUE) {
    if (g_n > 0) {
      if (g_det_w == w && g_det_h == h) {
        seg.draw(rgb, w, h, g_disp, g_n);
      } else if (g_det_w > 0 && g_det_h > 0) {
        esp32p4_seg_t scaled[8];
        int n = 0;
        for (int i = 0; i < g_n && n < 8; i++) {
          scaled[n] = g_disp[i];
          scaled[n].box.x = g_disp[i].box.x * w / g_det_w;
          scaled[n].box.y = g_disp[i].box.y * h / g_det_h;
          scaled[n].box.w = g_disp[i].box.w * w / g_det_w;
          scaled[n].box.h = g_disp[i].box.h * h / g_det_h;
          scaled[n].mask = nullptr;
          scaled[n].mask_w = 0;
          scaled[n].mask_h = 0;
          n++;
        }
        if (n > 0) seg.draw(rgb, w, h, scaled, n);
      }
    }
    xSemaphoreGive(g_res_mu);
  }
  uint32_t gap = 800;
  if (ui.ms > 0) gap = (uint32_t)ui.ms / 4u + 250u;
  if (gap < 500) gap = 500;
  if (gap > 2000) gap = 2000;
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
  }
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-seg");
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
  wfm->setName("Seg models").setPorts(WFM_UI_PORT, WFM_FILE_PORT).setHomePort(CAM_UI_PORT);
  if (!wfm->begin()) {
    Serial.println("WebFileManager begin FAILED");
    return false;
  }
  wfm->startFileTask();
  return true;
}

static void printUploadHelp(const IPAddress &ip) {
  Serial.println();
  Serial.printf("Missing seg .espdl on %s — camera UI is up; upload via Files:\n", store.label());
  Serial.printf("  Camera http://%s/\n", ip.toString().c_str());
  Serial.printf("  Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("  Put under /%s/models/p4/  (coco_seg_yolo11n_seg_s8_v1.espdl)\n", store.label());
}

static void markNeedModel() {
  stream.setModelMissingNote(store.volumeSummary());
  snprintf(stream.detUi().summary, sizeof(stream.detUi().summary), "need model · Files :%u",
           WFM_UI_PORT);
}

static bool tryBeginSeg() {
  if (det_ready) return true;
  if (!modelsPresent()) return false;
  stream.setEncodePaused(true);
  bool ok = seg.begin();
  stream.setEncodePaused(false);
  if (!ok) return false;
  seg.setScoreThr((float)stream.detUi().thr_pct / 100.f);
  det_ready = true;
  stream.setPreviewNote("");
  stream.detUi().summary[0] = '\0';
  return true;
}

static void tryLoadModel() {
  static uint32_t last = 0;
  static bool reboot_hint = false;
  if (det_ready) return;
  if (millis() - last < 2000) return;
  last = millis();
  if (!modelsPresent()) return;
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  if (largest < 6 * 1024 * 1024) {
    if (!reboot_hint) {
      reboot_hint = true;
      Serial.printf("Seg: weights on SD but PSRAM largest=%u KB (need ~6MB). Reboot to load before camera.\n",
                    (unsigned)(largest / 1024));
    }
    return;
  }
  if (tryBeginSeg()) Serial.println("Seg: model loaded");
}

static bool startCameraDet() {
  auto &ui = stream.detUi();
  if (modelsPresent()) {
    Serial.println("Seg: loading YOLO11n-Seg before camera (needs ~6MB PSRAM)");
    if (!tryBeginSeg()) Serial.println("Seg: begin FAILED — camera UI still up");
  }

  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  if (cfg.frame_size == ESP32P4_FRAMESIZE_AUTO || cfg.frame_size == ESP32P4_FRAMESIZE_1080P ||
      cfg.frame_size == ESP32P4_FRAMESIZE_2304X1296 || cfg.frame_size == ESP32P4_FRAMESIZE_5MP) {
    cfg.frame_size = ESP32P4_FRAMESIZE_HD;
    Serial.println("Seg: capture HD 1280x720 so FBs leave a PSRAM slab for the model");
  }
  if (cfg.fb_count > 2) cfg.fb_count = 2;

  if (!cam.begin(cfg)) {
    Serial.println("camera FAILED");
    return false;
  }
  /* IPA AE only — sensor AGC on RAW8 first frames is black / magenta. */
  if (cam.ispReady()) {
    cam.setAEC(false);
    cam.setAGC(false);
    cam.setIspAe(true);
  }
  cam.setGainCeiling(80);
  uint32_t t_ae = millis();
  int warmed = 0;
  while (warmed < 30 && millis() - t_ae < 2500) {
    camera_fb_t *fb = cam.capture(120);
    if (!fb) break;
    cam.release(fb);
    warmed++;
  }
  Serial.printf("Seg: AE warmup %d frames\n", warmed);

  stream.setEncodePaused(true);
  stream.begin(&cam, CAM_UI_PORT, 38);
  stream.setFramesize(ESP32P4_STREAM_HD);
  stream.enableDetUi(true);
  stream.setDetCatalog("Seg", "COCO seg", "YOLO11n-Seg instance masks · /models/p4/", kN, kVals, kLabs);
  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setFrameHook(detHook, nullptr);
  loadSettings();
  if (!det_ready) {
    markNeedModel();
    printUploadHelp(ETH.localIP());
  } else {
    seg.setScoreThr((float)ui.thr_pct / 100.f);
  }
  g_res_mu = xSemaphoreCreateMutex();
  g_det_rgb = (uint16_t *)esp32p4_psram_alloc(640u * 360u * 2u);
  g_det_job = xSemaphoreCreateBinary();
  BaseType_t task_ok = pdFAIL;
  if (g_det_rgb && g_det_job && g_res_mu) {
#ifdef CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM
    task_ok = xTaskCreatePinnedToCoreWithCaps(detTask, "coco_seg", 32768, nullptr, 1, nullptr, 0,
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    if (task_ok != pdPASS) {
      task_ok = xTaskCreatePinnedToCore(detTask, "coco_seg", 6144, nullptr, 1, nullptr, 0);
    }
  }
  if (!g_det_rgb || !g_det_job || !g_res_mu || task_ok != pdPASS) {
    Serial.printf("Seg: async FAILED rgb=%d job=%d mu=%d task=%d\n", g_det_rgb ? 1 : 0,
                  g_det_job ? 1 : 0, g_res_mu ? 1 : 0, (int)task_ok);
  }
  if (!cam.ispReady()) stream.enableSmartAe(true);
  stream.setEncodePaused(false);
  saveSettings();
  IPAddress ip = ETH.localIP();
  Serial.printf("UI     http://%s/\n", ip.toString().c_str());
  Serial.printf("Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("Seg ready=%d thr=%d%%\n", seg.ready() ? 1 : 0, ui.thr_pct);
  return true;
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 38_EthSegWeb (ESP-DL COCO seg) ===");
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
  tryLoadModel();
  auto &ui = stream.detUi();
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }
  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    Serial.printf("[hb] segs=%d ms=%d det=%d %s\n", ui.objs, ui.ms, ui.detect_en ? 1 : 0,
                  ui.summary[0] ? ui.summary : "-");
  }
}
