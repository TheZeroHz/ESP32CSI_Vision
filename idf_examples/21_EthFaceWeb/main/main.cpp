/**
 * 21_EthFaceWeb — Ethernet MJPEG web UI + official ESP-DL face detect/recognize
 *
 * Models (Espressif Component Registry):
 *   - espressif/human_face_detect        MSR+MNP / ESPDet Pico
 *   - espressif/human_face_recognition   MFN_S8_V1 feature + DB
 *
 * Board: Guition JC-ESP32P4-M3 (IP101 + CSI OV5647)
 *
 * Build (ESP-IDF 5.4/5.5 — NOT Arduino IDE):
 *   cd idf_examples/21_EthFaceWeb
 *   idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
 *   idf.py -p COM8 build flash monitor
 *
 * Open http://<ip>/ → Face panel: model select, Enroll, Clear DB.
 * Stream shows cyan detect boxes / yellow ID# when recognized.
 */

#include <Arduino.h>

#ifndef ETH_PHY_MDC
#define ETH_PHY_TYPE  ETH_PHY_IP101
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   31
#define ETH_PHY_MDIO  52
#define ETH_PHY_POWER 51
#define ETH_CLK_MODE  EMAC_CLK_EXT_IN
#endif

#include <ETH.h>
#include <ESP32CSI_Vision.h>
#include "ESP32P4_FaceAi.h"

#include "esp_spiffs.h"
#include "esp_log.h"

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;
ESP32P4_FaceAi face;

static volatile bool eth_ready = false;
static esp32p4_face_id_t g_faces[8];
static portMUX_TYPE g_face_mux = portMUX_INITIALIZER_UNLOCKED;
static int g_face_n = 0;
static int g_skip = 0;

static bool mountSpiffs() {
  esp_vfs_spiffs_conf_t conf = {};
  conf.base_path = "/spiffs";
  conf.partition_label = "storage";
  conf.max_files = 4;
  conf.format_if_mount_failed = true;
  esp_err_t e = esp_vfs_spiffs_register(&conf);
  if (e != ESP_OK) {
    Serial.printf("SPIFFS mount failed: %s\n", esp_err_to_name(e));
    return false;
  }
  size_t total = 0, used = 0;
  esp_spiffs_info("storage", &total, &used);
  Serial.printf("SPIFFS ok total=%u used=%u\n", (unsigned)total, (unsigned)used);
  return true;
}

static ESP32P4_FaceDetect::Model modelFromUi(int m) {
  switch (m) {
    case 1:
      return ESP32P4_FaceDetect::ESPDET_PICO_224;
    case 2:
      return ESP32P4_FaceDetect::ESPDET_PICO_416;
    default:
      return ESP32P4_FaceDetect::MSRMNP_S8_V1;
  }
}

static void faceHook(uint16_t *rgb, int w, int h, void * /*user*/) {
  auto &ui = stream.faceUi();

  if (ui.clear_req) {
    ui.clear_req = false;
    face.clearDb();
    ui.feats = 0;
  }
  if (ui.enroll_req) {
    face.requestEnroll();
    ui.enroll_req = false;
  }
  if (ui.model_req) {
    ui.model_req = false;
    // Re-init detector+recognizer with new model (keeps same DB path)
    face.end();
    face.begin(modelFromUi(ui.model), "/spiffs/face.db");
  }

  // Every other stream frame — keep encode responsive
  if ((++g_skip & 1) == 0) {
    int n = face.run(rgb, w, h, g_faces, 8, true);
    portENTER_CRITICAL(&g_face_mux);
    g_face_n = n;
    portEXIT_CRITICAL(&g_face_mux);
    ui.faces = face.lastCount();
    ui.ms = face.lastMs();
    ui.feats = face.featCount();
  }

  int n;
  portENTER_CRITICAL(&g_face_mux);
  n = g_face_n;
  portEXIT_CRITICAL(&g_face_mux);
  if (n > 0) ESP32P4_FaceAi::draw(rgb, w, h, g_faces, n);
}

void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname("esp32p4-face");
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

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 21_EthFaceWeb (ESP-DL face + Ethernet MJPEG) ===");

  if (!mountSpiffs()) {
    Serial.println("continuing without FR DB (detect-only if begin fails FR)");
  }

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  if (!face.begin(ESP32P4_FaceDetect::MSRMNP_S8_V1, "/spiffs/face.db")) {
    Serial.println("FaceAi.begin FAILED");
    while (true) delay(1000);
  }
  Serial.printf("FaceAi ready  det=MSR+MNP  rec=%d\n", face.recognitionReady() ? 1 : 0);

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

  if (!stream.begin(&cam, 80, 38)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }
  stream.setFramesize(ESP32P4_STREAM_VGA);  // 400×320
  stream.enableFaceUi(true);
  stream.setFrameHook(faceHook, nullptr);

  IPAddress ip = ETH.localIP();
  Serial.printf("UI      http://%s/\n", ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("Face panel: Enroll / Clear. Yellow ID = recognized.");
}

void loop() {
  stream.loop();
  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    auto &ui = stream.faceUi();
    Serial.printf("[hb] faces=%d ms=%d sent=%u\n", ui.faces, ui.ms, (unsigned)stream.sent());
  }
}
