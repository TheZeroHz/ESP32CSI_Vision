/**
 * 22_EthQrWeb — Ethernet MJPEG + QR/barcode scan (zxing-cpp + ESP32-P4 PPA)
 *
 * Board: Guition JC-ESP32P4-M3 (IP101 Ethernet + CSI)
 *
 * Open http://<ip>/ → QR tab:
 *   SCAN + per-format checkboxes (saved to SD / FFat / flash)
 *   Last type + payload
 *
 * Storage: ESP32P4_STORAGE_AUTO → /qr/settings.txt (+ NVS backup)
 */

#include <Arduino.h>


#ifndef QR_DECODE_INTERVAL_MS
#define QR_DECODE_INTERVAL_MS 700
#endif

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <Preferences.h>
#include "board_config.h"
#include <ETH.h>

#ifndef APP_NAME
#define APP_NAME "22_EthQrWeb"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif


ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_Qr qr;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_Ppa ppa;
Preferences qrPrefs;

static volatile bool eth_ready = false;
static bool store_ok = false;
static esp32p4_qr_code_t g_codes[ESP32P4_QR_MAX_CODES];
static portMUX_TYPE g_qr_mux = portMUX_INITIALIZER_UNLOCKED;

static uint8_t *g_gray = nullptr;
static size_t g_gray_cap = 0;
static volatile int g_gray_w = 0;
static volatile int g_gray_h = 0;
static volatile float g_scale_x = 2.0f;
static volatile float g_scale_y = 2.0f;
static volatile bool g_qr_busy = false;
static SemaphoreHandle_t g_qr_job = nullptr;
static TaskHandle_t g_qr_task = nullptr;
static uint32_t g_last_kick_ms = 0;

static const char *kSettingsFs = "/qr/settings.txt";

static void applyFormatsToDecoder() {
  auto &ui = stream.qrUi();
  uint32_t m = ui.formats ? ui.formats : qr.defaultFormats();
  qr.setFormats(m);
}

static void saveSettingsNvs() {
  auto &ui = stream.qrUi();
  if (!qrPrefs.begin("p4qr", false)) return;
  qrPrefs.putUInt("fmts", ui.formats ? ui.formats : qr.defaultFormats());
  qrPrefs.putBool("scan", ui.scan_en);
  qrPrefs.end();
}

static void saveSettings() {
  auto &ui = stream.qrUi();
  uint32_t fmts = ui.formats ? ui.formats : qr.defaultFormats();
  char buf[96];
  snprintf(buf, sizeof(buf), "scan=%d\nformats=%u\n", ui.scan_en ? 1 : 0, (unsigned)fmts);

  if (store_ok) {
    if (!store.exists("/qr")) store.mkdir("/qr");
    File f = store.fs().open(kSettingsFs, FILE_WRITE);
    if (f) {
      f.print(buf);
      f.flush();
      f.close();
      Serial.printf("QR settings: saved %s (%s) scan=%d fmts=%u\n", kSettingsFs, store.label(),
                    ui.scan_en ? 1 : 0, (unsigned)fmts);
    } else {
      Serial.printf("QR settings: write failed %s\n", kSettingsFs);
    }
  }
  saveSettingsNvs();
  applyFormatsToDecoder();
}

static void loadSettings() {
  auto &ui = stream.qrUi();
  ui.formats = qr.defaultFormats();
  ui.scan_en = true;

  bool loaded = false;
  if (store_ok && store.exists(kSettingsFs)) {
    File f = store.fs().open(kSettingsFs, FILE_READ);
    if (f) {
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int eq = line.indexOf('=');
        if (eq <= 0) continue;
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim();
        val.trim();
        if (key == "scan") ui.scan_en = val.toInt() != 0;
        else if (key == "formats") {
          uint32_t m = (uint32_t)val.toInt();
          m &= qr.defaultFormats();
          if (m) ui.formats = m;
        }
      }
      f.close();
      loaded = true;
      Serial.printf("QR settings: loaded %s (%s) scan=%d fmts=%u\n", kSettingsFs, store.label(),
                    ui.scan_en ? 1 : 0, (unsigned)ui.formats);
    }
  }

  if (!loaded && qrPrefs.begin("p4qr", true)) {
    ui.formats = qrPrefs.getUInt("fmts", qr.defaultFormats());
    ui.formats &= qr.defaultFormats();
    if (!ui.formats) ui.formats = qr.defaultFormats();
    ui.scan_en = qrPrefs.getBool("scan", true);
    qrPrefs.end();
    Serial.printf("QR settings: loaded NVS scan=%d fmts=%u\n", ui.scan_en ? 1 : 0,
                  (unsigned)ui.formats);
  }

  applyFormatsToDecoder();
}

static void qrTask(void * /*arg*/) {
  esp32p4_qr_code_t local[ESP32P4_QR_MAX_CODES];
  for (;;) {
    if (xSemaphoreTake(g_qr_job, portMAX_DELAY) != pdTRUE) continue;
    applyFormatsToDecoder();
    const int w = g_gray_w;
    const int h = g_gray_h;
    const float sx = g_scale_x;
    const float sy = g_scale_y;
    int n = 0;
    int ms = 0;
    const uint32_t t0 = millis();
    if (g_gray && w >= 16 && h >= 16 && qr.ready()) {
      n = qr.scanGray(g_gray, w, h, sx, sy, local, ESP32P4_QR_MAX_CODES);
      ms = qr.lastMs();
    } else {
      ms = (int)(millis() - t0);
    }

    auto &ui = stream.qrUi();
    portENTER_CRITICAL(&g_qr_mux);
    ui.codes = n;
    ui.ms = ms > 0 ? ms : 1;
    if (n > 0) {
      memcpy(g_codes, local, sizeof(esp32p4_qr_code_t) * (size_t)n);
      strncpy(ui.payload, local[0].payload, sizeof(ui.payload) - 1);
      ui.payload[sizeof(ui.payload) - 1] = '\0';
      strncpy(ui.format_name, qr.formatName(local[0].format), sizeof(ui.format_name) - 1);
      ui.format_name[sizeof(ui.format_name) - 1] = '\0';
    }
    portEXIT_CRITICAL(&g_qr_mux);

    if (n > 0) {
      Serial.printf("BC[%d] %s: %s (%dms)\n", n, qr.formatName(local[0].format),
                    local[0].payload, ms);
    } else {
      Serial.printf("BC: none (%dms %dx%d gray)\n", ms, w, h);
    }
    g_qr_busy = false;
  }
}

static bool ensureGraySnap(int w, int h) {
  size_t need = (size_t)w * (size_t)h;
  if (g_gray && need <= g_gray_cap) return true;
  if (g_gray) {
    esp32p4_psram_free(g_gray);
    g_gray = nullptr;
    g_gray_cap = 0;
  }
  g_gray = (uint8_t *)esp32p4_psram_alloc(need);
  if (!g_gray) return false;
  g_gray_cap = need;
  return true;
}

static void onQrFrame(uint16_t *rgb, int w, int h, void * /*user*/) {
  auto &ui = stream.qrUi();
  if (!ui.scan_en || !qr.ready()) {
    ui.codes = 0;
    ui.ms = 0;
    return;
  }

  portENTER_CRITICAL(&g_qr_mux);
  int n = ui.codes;
  esp32p4_qr_code_t local[ESP32P4_QR_MAX_CODES];
  if (n > 0) memcpy(local, g_codes, sizeof(esp32p4_qr_code_t) * (size_t)n);
  portEXIT_CRITICAL(&g_qr_mux);
  if (n > 0) qr.draw(rgb, w, h, local, n);

  const uint32_t now = millis();
  if (g_qr_busy && (now - g_last_kick_ms) > 5000) {
    Serial.println("BC: decode watchdog - reset busy");
    g_qr_busy = false;
  }
  if (g_qr_busy || !g_qr_job) return;
  if (now - g_last_kick_ms < QR_DECODE_INTERVAL_MS) return;

  int dw = w / 2;
  int dh = h / 2;
  if (dw < 160) {
    dw = w;
    dh = h;
  }
  if (!ensureGraySnap(dw, dh)) return;
  if (!ppa.rgb565ToGrayScale(rgb, w, h, g_gray, dw, dh)) return;

  g_gray_w = dw;
  g_gray_h = dh;
  g_scale_x = (float)w / (float)dw;
  g_scale_y = (float)h / (float)dh;
  g_qr_busy = true;
  g_last_kick_ms = now;
  ui.ms = -1;
  xSemaphoreGive(g_qr_job);
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-qr");
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

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 22_EthQrWeb (QR + Ethernet MJPEG) ===");
  dbg.begin(APP_NAME, APP_DEBUG);

  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  store_ok = store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD);
  if (store_ok) {
    Serial.printf("Storage: %s\n", store.label());
  } else {
    Serial.println("Storage mount FAILED - using NVS only for QR settings");
  }

  Network.onEvent(onEthEvent);
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER,
                 ETH_CLK_MODE)) {
    Serial.println("ETH.begin FAILED");
    while (true) delay(1000);
  }

  Serial.println("Waiting for DHCP...");
  uint32_t t0 = millis();
  while (!eth_ready && millis() - t0 < 30000) delay(200);
  if (!eth_ready) {
    Serial.println("no Ethernet IP");
    while (true) delay(1000);
  }

  if (!qr.begin(640, 360, false)) {
    Serial.println("QR (zxing-cpp + PPA) begin FAILED (PSRAM?)");
    while (true) delay(1000);
  }
  (void)ppa.begin();

  g_qr_job = xSemaphoreCreateBinary();
  if (!g_qr_job ||
      xTaskCreatePinnedToCore(qrTask, "p4cam_qr", 40960, nullptr, 1, &g_qr_task, 0) != pdPASS) {
    Serial.println("QR task FAILED");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 40)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }
  stream.enableSmartAe(!cam.ispReady());
  stream.setFramesize(ESP32P4_STREAM_HD);
  stream.enableQrUi(true);
  loadSettings();
  stream.setFrameHook(onQrFrame, nullptr);

  IPAddress ip = ETH.localIP();
  Serial.printf("UI      http://%s/   (QR formats + IPA AGC / Smart AE fallback)\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
}

void loop() {
  stream.loop();
  auto &ui = stream.qrUi();
  if (ui.settings_dirty) {
    ui.settings_dirty = false;
    saveSettings();
  }
  static uint32_t last = 0;
  if (millis() - last >= 2500) {
    last = millis();
    Serial.printf("[hb] sent=%u jpeg=%u qr_n=%d qr_ms=%d en=%d busy=%d fmts=%u\n",
                  (unsigned)stream.sent(), (unsigned)stream.lastJpegBytes(), ui.codes, ui.ms,
                  ui.scan_en ? 1 : 0, g_qr_busy ? 1 : 0, (unsigned)ui.formats);
  }
}
