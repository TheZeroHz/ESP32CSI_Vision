/**
 * 21_EthFaceWeb — Ethernet MJPEG + official ESP-DL face detect/recognize
 *
 * Vendored under src/espdl/ (no separate ESP-IDF project required).
 *
 * Storage preference (edit APP_STORAGE below):
 *   ESP32P4_STORAGE_AUTO      SD → FFat → LittleFS → SPIFFS
 *   ESP32P4_STORAGE_SD        microSD only
 *   ESP32P4_STORAGE_FFAT      flash FAT (needs partition e.g. app3M_fat9M_16MB)
 *   ESP32P4_STORAGE_LITTLEFS  flash LittleFS
 *   ESP32P4_STORAGE_SPIFFS    flash SPIFFS
 *
 * On the chosen volume:
 *   /models/p4/*.espdl
 *   /face/face.db · /face/names.txt · /face/settings.txt
 *   Capture → /IMG/…   Record → /VIDEO/…
 * VFS (fopen / ESP-DL): {/sdcard|/ffat|/littlefs}/…
 *
 * If models missing, WebFileManager stays up so you can upload them.
 * Secondary volume is added to WFM when available (e.g. FFat + SD).
 *
 * Ports: :80 Camera/Face UI · :82 WebFileManager · :83 WFM transfers
 * Board: Guition JC-ESP32P4-M3
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

/** Prefer SD for large video; set ESP32P4_STORAGE_FFAT to use flash FAT instead. */
#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <Arduino.h>
#include <FS.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


#include "board_config.h"
#include <ETH.h>

#ifndef APP_NAME
#define APP_NAME "21_EthFaceWeb"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif


static const uint16_t CAM_UI_PORT = 80;
static const uint16_t WFM_UI_PORT = 82;
static const uint16_t WFM_FILE_PORT = 83;

// H.264 encode size for Record (independent of MJPEG / face lock size).
static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

// Arduino FS paths (root-relative on the selected volume)
static const char *kMsr = "/models/p4/human_face_detect_msr_s8_v1.espdl";
static const char *kMnp = "/models/p4/human_face_detect_mnp_s8_v1.espdl";
static const char *kMfn = "/models/p4/human_face_feat_mfn_s8_v1.espdl";
static const char *kFaceDir = "/face";
static const char *kDbFs = "/face/face.db";
static const char *kNamesFs = "/face/names.txt";
static const char *kDbLegacyFs = "/face.db";
static const char *kNamesLegacyFs = "/face_names.txt";
static const char *kSettingsFs = "/face/settings.txt";

// fopen / ESP-DL VFS paths (filled after store.begin)
static char kFaceDirVfs[48];
static char kDb[64];
static char kNames[64];
static char kDbLegacy[64];
static char kNamesLegacy[64];
static char kSettings[64];

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_H264 h264;
ESP32P4_Mic mic;
ESP32P4_FaceAi face;

WfmStorageFS *appVol = nullptr;
WebFileManager *wfm = nullptr;

static volatile bool eth_ready = false;
static bool face_ready = false;
static esp32p4_face_id_t g_faces[8];
static portMUX_TYPE g_face_mux = portMUX_INITIALIZER_UNLOCKED;
static int g_face_n = 0;

static void rebuildVfsPaths() {
  store.vfsPath(kFaceDirVfs, sizeof(kFaceDirVfs), "/face");
  store.vfsPath(kDb, sizeof(kDb), "/face/face.db");
  store.vfsPath(kNames, sizeof(kNames), "/face/names.txt");
  store.vfsPath(kDbLegacy, sizeof(kDbLegacy), "/face.db");
  store.vfsPath(kNamesLegacy, sizeof(kNamesLegacy), "/face_names.txt");
  store.vfsPath(kSettings, sizeof(kSettings), "/face/settings.txt");
}

static void saveSettings() {
  if (!store.exists(kFaceDir)) store.mkdir(kFaceDir);
  auto &ui = stream.faceUi();
  bool hm = false, vf = false, aec = true, agc = true;
  uint16_t exp = 100, gain = 16, ceil = 248;
  cam.getHMirror(&hm);
  cam.getVFlip(&vf);
  cam.getAEC(&aec);
  cam.getAGC(&agc);
  cam.getExposure(&exp);
  cam.getGain(&gain);
  cam.getGainCeiling(&ceil);
  if (exp > 980) exp = 980;
  int colorbar = cam.testPattern() ? 1 : 0;
  char buf[384];
  snprintf(buf, sizeof(buf),
           "detect=%d\nrecog=%d\nmodel=%d\nthr=%d\n"
           "quality=%u\nframesize=%u\nframeskip=%u\n"
           "hmirror=%d\nvflip=%d\n"
           "aec=%d\nagc=%d\naec_value=%u\nagc_gain=%u\ngainceiling=%u\n"
           "colorbar=%d\n",
           ui.detect_en ? 1 : 0, ui.recog_en ? 1 : 0, ui.model, ui.thr_pct,
           (unsigned)stream.quality(), (unsigned)stream.framesize(),
           (unsigned)stream.frameSkip(), hm ? 1 : 0, vf ? 1 : 0, aec ? 1 : 0, agc ? 1 : 0,
           (unsigned)exp, (unsigned)gain, (unsigned)ceil, colorbar);

  // FILE_WRITE truncates → always overwrite on change.
  File f = store.fs().open(kSettingsFs, FILE_WRITE);
  if (f) {
    f.print(buf);
    f.flush();
    f.close();
    Serial.printf("settings: overwritten %s (%s) det=%d rec=%d thr=%d q=%u fs=%u skip=%u\n",
                  kSettingsFs, store.label(), ui.detect_en ? 1 : 0, ui.recog_en ? 1 : 0, ui.thr_pct,
                  (unsigned)stream.quality(), (unsigned)stream.framesize(),
                  (unsigned)stream.frameSkip());
    return;
  }
  FILE *fp = fopen(kSettings, "w");
  if (!fp) {
    Serial.printf("settings: cannot write %s\n", kSettingsFs);
    return;
  }
  fputs(buf, fp);
  fflush(fp);
  fsync(fileno(fp));
  fclose(fp);
  Serial.printf("settings: overwritten (vfs) %s\n", kSettings);
}

static void loadSettings();  // defined after syncFaceUiStatus

static bool modelsPresent() {
  return store.exists(kMsr) && store.exists(kMnp);
}

static void ensureModelDirs() {
  if (!store.exists("/models")) store.mkdir("/models");
  if (!store.exists("/models/p4")) store.mkdir("/models/p4");
}

/** Face templates on preferred storage · loaded to PSRAM at begin().
 *  FS paths are root-relative (/face/...); fopen uses VFS ({mount}/face/...). */
static void ensureFaceStore() {
  if (!store.exists(kFaceDir)) store.mkdir(kFaceDir);
  mkdir(kFaceDirVfs, 0775);

  auto copyFile = [](const char *from_vfs, const char *to_vfs) -> bool {
    FILE *in = fopen(from_vfs, "rb");
    if (!in) return false;
    FILE *out = fopen(to_vfs, "wb");
    if (!out) {
      fclose(in);
      return false;
    }
    char buf[512];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
      if (fwrite(buf, 1, n, out) != n) {
        ok = false;
        break;
      }
    }
    fflush(out);
    fsync(fileno(out));
    fclose(in);
    fclose(out);
    if (!ok) remove(to_vfs);
    return ok;
  };

  // Prefer /face/face.db; migrate legacy root face.db when needed.
  if (!store.exists(kDbFs) && store.exists(kDbLegacyFs)) {
    if (!copyFile(kDbLegacy, kDb)) {
      Serial.println("Face store: legacy DB migrate failed");
    }
  }
  if (!store.exists(kNamesFs) && store.exists(kNamesLegacyFs)) {
    copyFile(kNamesLegacy, kNames);
  }

  // Touch empty DB so DataBase/enroll can open with rb+.
  if (!store.exists(kDbFs)) {
    FILE *f = fopen(kDb, "wb");
    if (f) fclose(f);
  }

  File f = store.fs().open(kDbFs, FILE_READ);
  long sz = f ? (long)f.size() : -1;
  if (f) f.close();
  Serial.printf("Face store: %s (fs %s) exists=%d size=%ld\n", kDb, kDbFs,
                store.exists(kDbFs) ? 1 : 0, sz);
}

static int modelFromUi(int m) {
  switch (m) {
    case 1: return ESP32P4_FACE_ESPDET_PICO_224;
    case 2: return ESP32P4_FACE_ESPDET_PICO_416;
    default: return ESP32P4_FACE_MSRMNP_S8_V1;
  }
}

static void syncFaceUiStatus() {
  auto &ui = stream.faceUi();
  ui.feats = face.featCount();
  ui.enroll_ok = face.lastEnrollStatus();
  ui.enroll_id = face.lastEnrollId();
  ui.enroll_got = face.enrollSamplesDone();
  ui.enroll_need = face.enrollSamplesNeed();
  // Keep ui.thr_pct as set by /control + settings file (don't round-trip float and clobber).
  face.rosterText(ui.roster, sizeof(ui.roster));
  strncpy(ui.db_path, kDb, sizeof(ui.db_path) - 1);
  ui.db_path[sizeof(ui.db_path) - 1] = '\0';
}

static void loadSettings() {
  FILE *f = fopen(kSettings, "r");
  if (!f) {
    Serial.println("settings: none yet (defaults)");
    return;
  }
  char line[80];
  int detect = 0, recog = 0, model = 0, thr = 50, quality = 38, framesize = 1, frameskip = 0;
  int hmirror = 0, vflip = 0, aec = 1, agc = 1, colorbar = 0;
  int aec_value = 100, agc_gain = 16, gainceiling = 248;
  while (fgets(line, sizeof(line), f)) {
    int v = 0;
    if (sscanf(line, "detect=%d", &v) == 1) detect = v;
    else if (sscanf(line, "recog=%d", &v) == 1) recog = v;
    else if (sscanf(line, "model=%d", &v) == 1) model = v;
    else if (sscanf(line, "thr=%d", &v) == 1) thr = v;
    else if (sscanf(line, "quality=%d", &v) == 1) quality = v;
    else if (sscanf(line, "framesize=%d", &v) == 1) framesize = v;
    else if (sscanf(line, "frameskip=%d", &v) == 1) frameskip = v;
    else if (sscanf(line, "hmirror=%d", &v) == 1) hmirror = v;
    else if (sscanf(line, "vflip=%d", &v) == 1) vflip = v;
    else if (sscanf(line, "aec=%d", &v) == 1) aec = v;
    else if (sscanf(line, "agc=%d", &v) == 1) agc = v;
    else if (sscanf(line, "aec_value=%d", &v) == 1) aec_value = v;
    else if (sscanf(line, "agc_gain=%d", &v) == 1) agc_gain = v;
    else if (sscanf(line, "gainceiling=%d", &v) == 1) gainceiling = v;
    else if (sscanf(line, "colorbar=%d", &v) == 1) colorbar = v;
  }
  fclose(f);

  if (thr < 10) thr = 10;
  if (thr > 95) thr = 95;
  if (quality < 4) quality = 4;
  if (quality > 63) quality = 63;
  if (framesize < 0) framesize = 0;
  if (framesize >= (int)ESP32P4_STREAM_COUNT) framesize = 1;
  if (frameskip < 0) frameskip = 0;
  if (frameskip > 8) frameskip = 8;
  if (model < 0 || model > 2) model = 0;
  if (aec_value < 4) aec_value = 4;
  if (aec_value > 980) aec_value = 980;
  if (agc_gain < 0) agc_gain = 0;
  if (agc_gain > 1023) agc_gain = 1023;
  if (gainceiling < 16) gainceiling = 16;
  if (gainceiling > 1023) gainceiling = 1023;
  if (recog) detect = 1;

  auto &ui = stream.faceUi();
  ui.detect_en = detect != 0;
  ui.recog_en = recog != 0;
  ui.model = model;
  ui.thr_pct = thr;
  stream.setQuality((uint8_t)quality);
  stream.setFrameSkip((uint8_t)frameskip);
  stream.setFramesize((uint8_t)framesize);
  cam.setHMirror(hmirror != 0);
  cam.setVFlip(vflip != 0);
  // Manual exposure/gain first, then auto switches (so AEC/AGC off restores saved values).
  cam.setExposure((uint16_t)aec_value);
  cam.setGain((uint16_t)agc_gain);
  cam.setGainCeiling((uint16_t)gainceiling);
  cam.setAEC(aec != 0);
  cam.setAGC(agc != 0);
  cam.setTestPattern(colorbar != 0);
  face.setThresh((float)thr / 100.f);
  if (model != 0) {
    face.end();
    face.begin(modelFromUi(model), kDb, kNames);
    face.setThresh((float)thr / 100.f);
  }
  stream.syncFaceStreamSize();
  syncFaceUiStatus();
  Serial.printf("settings: loaded det=%d rec=%d model=%d thr=%d q=%d fs=%d skip=%d aec=%d agc=%d\n",
                detect, recog, model, thr, quality, framesize, frameskip, aec, agc);
}

static void faceHook(uint16_t *rgb, int w, int h, void *) {
  if (!face_ready) return;

  auto &ui = stream.faceUi();

  if (ui.clear_req) {
    ui.clear_req = false;
    face.cancelEnroll();
    face.clearDb();
    syncFaceUiStatus();
  }
  if (ui.enroll_cancel) {
    ui.enroll_cancel = false;
    face.cancelEnroll();
    syncFaceUiStatus();
  }
  if (ui.delete_req) {
    ui.delete_req = false;
    int id = ui.delete_id;
    ui.delete_id = 0;
    if (id > 0) face.deleteId((uint16_t)id);
    syncFaceUiStatus();
  }
  if (ui.delete_name_req) {
    ui.delete_name_req = false;
    if (ui.delete_name[0]) face.deleteName(ui.delete_name);
    ui.delete_name[0] = '\0';
    syncFaceUiStatus();
  }
  if (ui.thr_req) {
    ui.thr_req = false;
    face.setThresh((float)ui.thr_pct / 100.f);
  }
  if (ui.enroll_req) {
    ui.enroll_req = false;
    // Enroll needs MSR+MNP landmarks. Model switch is done by the UI before enroll_req
    // — never mutate ui.model here (that was resetting ESPDet → MSR when recog raced enroll).
    ui.detect_en = true;
    // Do NOT clear ui.recog_en here; FaceAi already skips recognize while enrolling.
    stream.syncFaceStreamSize();
    face.requestEnroll(ui.enroll_name[0] ? ui.enroll_name : "face");
    syncFaceUiStatus();
  }
  if (ui.model_req) {
    ui.model_req = false;
    face.cancelEnroll();
    face.end();
    face.begin(modelFromUi(ui.model), kDb, kNames);
    face.setThresh((float)ui.thr_pct / 100.f);
    stream.syncFaceStreamSize();
    syncFaceUiStatus();
  }

  // After applying thr/model/toggles — then overwrite settings (never save stale values).
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }

  const bool enrolling = face.enrollPending();
  if (!ui.detect_en && enrolling) {
    face.cancelEnroll();
    syncFaceUiStatus();
  }
  if (!ui.detect_en && !face.enrollPending()) {
    portENTER_CRITICAL(&g_face_mux);
    g_face_n = 0;
    portEXIT_CRITICAL(&g_face_mux);
    ui.faces = 0;
    ui.ms = 0;
    syncFaceUiStatus();
    return;
  }

  // Always run while detect is on — skipping frames left stale boxes painted on new frames
  // (ghost overlays / boxes in black letterbox margins).
  bool do_recog = ui.recog_en && !face.enrollPending() && face.recognitionReady();
  int n = face.run(rgb, w, h, g_faces, 8, do_recog);
  portENTER_CRITICAL(&g_face_mux);
  g_face_n = n;
  portEXIT_CRITICAL(&g_face_mux);
  ui.faces = face.lastCount();
  ui.ms = face.lastMs();
  syncFaceUiStatus();

  if (n > 0) {
    face.draw(rgb, w, h, g_faces, n);
  }
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-face");
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
  wfm->setName("Face models")
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
  Serial.printf("Missing /models/p4/*.espdl on %s - upload via WebFileManager:\n", store.label());
  Serial.printf("  Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("  Put these under /%s/models/p4/\n", store.label());
  Serial.println("    human_face_detect_msr_s8_v1.espdl");
  Serial.println("    human_face_detect_mnp_s8_v1.espdl");
  Serial.println("    human_face_feat_mfn_s8_v1.espdl     (required for enroll/recognize)");
  Serial.println("Waiting for uploads...");
}

static bool startCameraFace() {
  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    return false;
  }

  if (!h264.begin(ENC_W, ENC_H, 30, ENC_BITRATE)) {
    Serial.println("H264 begin FAILED - Capture Img still works; Record disabled");
  }
  if (!mic.begin(16000)) {
    Serial.println("Mic FAILED - recordings will be video-only");
  }

  if (!face.begin(ESP32P4_FACE_MSRMNP_S8_V1, kDb, kNames)) {
    Serial.printf("FaceAi.begin FAILED — camera UI still up, upload via Files :%u\n",
                  (unsigned)WFM_UI_PORT);
    stream.setModelMissingNote(store.volumeSummary(), "Face models");
  } else {
    face_ready = true;
  }

  stream.begin(&cam, CAM_UI_PORT, 38);
  stream.setFramesize(ESP32P4_STREAM_HD);  // default until settings load
  stream.enableFaceUi(true);
  stream.setFilesBrowserPort(WFM_UI_PORT);
  stream.setFrameHook(faceHook, nullptr);

  if (!stream.enableCapture(&store.fs(), "/IMG")) {
    Serial.println("enableCapture FAILED");
  }
  if (h264.ready()) {
    if (!stream.enableVideoRecord(&store.fs(), &h264, "/VIDEO")) {
      Serial.println("enableVideoRecord FAILED");
    }
  }
  if (mic.ready()) stream.enableMic(&mic);

  loadSettings();
  syncFaceUiStatus();
  saveSettings();  // ensure file exists with current values

  IPAddress ip = ETH.localIP();
  Serial.printf("UI     http://%s/\n", ip.toString().c_str());
  Serial.printf("Files  http://%s:%u/\n", ip.toString().c_str(), (unsigned)WFM_UI_PORT);
  Serial.printf("Storage %s vfs=%s\n", store.label(), store.vfsRoot());
  Serial.printf("Capture /IMG  ok=%d\n", stream.sdCaptureEnabled() ? 1 : 0);
  Serial.printf("Record  /VIDEO ok=%d enc=%ux%u mic=%d\n", stream.videoRecordEnabled() ? 1 : 0,
                (unsigned)ENC_W, (unsigned)ENC_H, stream.micEnabled() ? 1 : 0);
  Serial.printf("FR     ready=%d feats=%d db=%s\n", face.recognitionReady() ? 1 : 0, face.featCount(),
                kDb);
  if (!store.exists(kMfn)) {
    Serial.println("WARN: MFN missing - detect works; enroll/recognize needs human_face_feat_mfn_s8_v1.espdl");
  }
  return true;
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 21_EthFaceWeb (vendored ESP-DL) ===");
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
  ensureFaceStore();

  if (!startEthAndWfm()) {
    while (true) delay(1000);
  }

  IPAddress ip = ETH.localIP();

  if (!startCameraFace()) {
    while (true) {
      wfm->loop();
      delay(2);
    }
  }
}

void loop() {
  stream.loop();
  if (wfm) wfm->loop();

  if (!face_ready && modelsPresent()) {
    static uint32_t last_try = 0;
    if (millis() - last_try >= 1500) {
      last_try = millis();
      stream.setEncodePaused(true);
      if (face.begin(ESP32P4_FACE_MSRMNP_S8_V1, kDb, kNames)) {
        face_ready = true;
        stream.setPreviewNote("");
        Serial.println("FaceAi: model loaded");
      }
      stream.setEncodePaused(false);
    }
  }

  // Backup path: save settings when face hook isn't running (detect off).
  auto &ui = stream.faceUi();
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }

  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    if (face_ready) {
      auto &ui = stream.faceUi();
      Serial.printf("[hb] faces=%d ms=%d feats=%d det=%d rec=%d enroll=%d saved=%u vids=%u recording=%d\n",
                    ui.faces, ui.ms, ui.feats, ui.detect_en ? 1 : 0, ui.recog_en ? 1 : 0, ui.enroll_ok,
                    (unsigned)stream.savedCount(), (unsigned)stream.videosSaved(),
                    stream.isRecording() ? 1 : 0);
    } else {
      Serial.println("[hb] waiting for models via WebFileManager");
    }
  }
}
