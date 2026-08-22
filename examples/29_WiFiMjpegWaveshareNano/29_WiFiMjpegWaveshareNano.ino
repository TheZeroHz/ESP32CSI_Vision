/**
 * 29_WiFiMjpegWaveshareNano — Waveshare ESP32-P4-Nano Wi-Fi MJPEG + mic audio
 *
 * Board: Waveshare ESP32-P4-Nano (OV5647 CSI, ESP32-C6 Wi-Fi, ES8311 mic, TF slot)
 *
 * Audio notes (for GitHub issue #EIKSEU):
 *   - MJPEG /stream is video-only (HTTP multipart JPEG).
 *   - The web UI shows a live mic waveform via /audio (enableMic).
 *   - Record/Stop saves H.264 + PCM → /VIDEO/*.mp4 on the TF card (needs SD + H264).
 *
 * Arduino-ESP32 3.3.x · Board: ESP32P4 Dev Module · PSRAM: Enabled
 * Partition: large app (e.g. app3M_fat9M_16MB on 16 MB flash)
 *
 * 1. Edit board_config.h (SSID + pins)
 * 2. Insert FAT32 TF card (required here because the example uses explicit SD config)
 * 3. Serial @ 115200 → open http://<ip>/
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif
#ifndef APP_NAME
#define APP_NAME "29_WiFiMjpegWaveshareNano"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif


#include "board_config.h"

static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;
static const uint32_t ENC_BITRATE = 1500000;

esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
esp32p4_sd_config_t sd_cfg = esp32csi_sd_config();
esp32p4_mic_config_t mic_cfg = esp32csi_mic_config();

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_H264 h264;
ESP32P4_Mic mic;
ESP32P4_MjpegServer stream;

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 29_WiFiMjpegWaveshareNano ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("board=%s  storage=%s\n", esp32csi_board_name(),
                store.kindName(APP_STORAGE));

  // Config-first path, matching camera/sd/mic helper style.
  cam_cfg.fb_count = 3;

  // Override SD pins here for custom carrier boards.
  // sd_cfg.clk = 43; sd_cfg.cmd = 44;
  // sd_cfg.d0 = 39; sd_cfg.d1 = 40; sd_cfg.d2 = 41; sd_cfg.d3 = 42;

  // Override mic pins / type here when your board differs from Waveshare Nano.
  // mic_cfg.type = ESP32P4_MIC_ES8311;
  // mic_cfg.i2c_sda = 7; mic_cfg.i2c_scl = 8;
  // mic_cfg.i2s_mclk = 13; mic_cfg.i2s_bclk = 12; mic_cfg.i2s_ws = 10;
  // mic_cfg.i2s_dout = 9; mic_cfg.i2s_din = 11; mic_cfg.pa_gpio = 53;
  mic_cfg.sample_rate = 16000;

  if (!cam.begin(cam_cfg)) {
    Serial.println("camera FAILED - check OV5647 CSI module");
    while (true) delay(1000);
  }

  if (!sd.begin(sd_cfg)) {
    Serial.println("sd.begin(cfg) FAILED - insert FAT32 TF card");
    while (true) delay(1000);
  }
  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage FAILED - insert FAT32 TF card or use a flash FAT partition");
    while (true) delay(1000);
  }
  Serial.printf("Storage %s vfs=%s\n", store.label(), store.vfsRoot());

  if (!h264.begin(ENC_W, ENC_H, 30, ENC_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  if (!mic.begin(mic_cfg)) {
    Serial.println("Mic FAILED - stream stays video-only (check mic type / ES8311 / I2S pins)");
  }

  esp32csi_wifi_config_t wifi_cfg = esp32csi_wifi_config();
  wifi_cfg.hostname = "esp32p4-nano-cam";
  wifi_cfg.ap_ssid = "ESP32-P4-Nano-Cam";
  if (!esp32csi_wifi_begin(wifi_cfg)) {
    Serial.println("Wi-Fi FAILED");
    while (true) delay(1000);
  }

  if (!stream.begin(&cam, 80, 35)) {
    Serial.println("mjpeg server FAILED");
    while (true) delay(1000);
  }

  stream.enableCapture(&store.fs(), "/IMG");
  if (!stream.enableVideoRecord(&store.fs(), &h264, "/VIDEO")) {
    Serial.println("enableVideoRecord FAILED");
    while (true) delay(1000);
  }
  if (mic.ready()) stream.enableMic(&mic);

  IPAddress ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
  Serial.printf("UI      http://%s/   (video + audio controls together)\n",
                ip.toString().c_str());
  Serial.printf("stream  http://%s:%u/stream   (video MJPEG)\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
  Serial.println("audio   UI waveform live now; Record/Stop -> MP4 with mic audio");
  Serial.printf("python  cam_wifi_viewer.py %s %u\n", ip.toString().c_str(),
                (unsigned)stream.streamPort());
}

void loop() {
  stream.loop();
  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("[hb] sent=%u rec=%u videos=%u mic=%s rms=%.2f\n", (unsigned)stream.sent(),
                  stream.isRecording() ? 1u : 0u, (unsigned)stream.videosSaved(),
                  mic.ready() ? "on" : "off", mic.ready() ? mic.rms() : 0.0f);
  }
}
