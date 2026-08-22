/**
 * 44_AvSdRecord — CSI video + ES8311 mic → one .mp4 (no Wi-Fi / Ethernet / MJPEG)
 *
 * Same idea as 11_H264SdRecord, plus microphone. The encoder writes H.264 as
 * fast as the pipeline allows; closeFile() remuxes wall-clock duration and
 * fuses PCM → AAC-LC. No live stream.
 *
 * Edit board_config.h for YOUR camera + SD + mic pins.
 * Speak near the board mic while it records.
 *
 * Serial @ 115200 · PSRAM on
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "44_AvSdRecord"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM | ESP32P4_DBG_H264 | ESP32P4_DBG_SD | ESP32P4_DBG_AUDIO
#endif

static const uint32_t RECORD_MS = 10000;  // wall-clock; auto-stop
static const uint32_t RECORD_BITRATE = 1500000;
static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_Ppa ppa;
ESP32P4_H264 h264;
ESP32P4_Mic mic;
ESP32P4_Debug dbg;
uint8_t *scaled = nullptr;

static bool nextMp4Path(char *out, size_t out_cap) {
  if (!store.exists("/VIDEO") && !store.mkdir("/VIDEO")) return false;
  for (uint32_t i = 1; i < 100000; i++) {
    snprintf(out, out_cap, "/VIDEO/VID_%05lu.mp4", (unsigned long)i);
    if (!store.exists(out)) return true;
  }
  return false;
}

/** Drain I2S into the PCM buffer (poll reads 256 samples per call). */
static void drainMic() {
  for (int i = 0; i < 16; i++) mic.poll();
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 44_AvSdRecord (MP4 + AAC, no stream) ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("APP_STORAGE pref=%s\n", store.kindName(APP_STORAGE));
  esp32csi_print_cam_config(esp32csi_cam_config());
  esp32csi_print_sd_config(esp32csi_sd_config());
  esp32csi_print_mic_config(esp32csi_mic_config());

  if (!cam.begin(esp32csi_cam_config())) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  ESP32P4_Sd *sd_ok = nullptr;
  if (sd.begin(esp32csi_sd_config())) {
    sd_ok = &sd;
  } else {
    Serial.println("SD begin failed — store will try flash");
  }
  if (!store.begin(APP_STORAGE, false, sd_ok)) {
    Serial.println("Storage FAILED");
    while (true) delay(1000);
  }
  Serial.printf("Storage %s vfs=%s\n", store.label(), store.vfsRoot());

  if (!ppa.begin()) {
    Serial.println("PPA FAILED");
    while (true) delay(1000);
  }

  const size_t scale_cap = (size_t)ENC_W * ENC_H * 2;
  scaled = (uint8_t *)esp32p4_psram_alloc(scale_cap);
  if (!scaled) {
    Serial.println("scale buffer FAILED");
    while (true) delay(1000);
  }

  if (!h264.begin(ENC_W, ENC_H, 30, RECORD_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  bool have_mic = mic.begin(esp32csi_mic_config());
  if (!have_mic) {
    Serial.println("mic FAILED — recording video-only");
  } else {
    // ~RECORD_MS + slack @ 16 kHz mono 16-bit (startPcmRam floors at 3 min)
    size_t pcm_cap = (size_t)mic.sampleRate() * 2u * ((RECORD_MS / 1000u) + 5u);
    if (!mic.startPcmRam(pcm_cap)) {
      Serial.println("PCM PSRAM FAILED — recording video-only");
      have_mic = false;
    }
  }

  char path[48];
  if (!nextMp4Path(path, sizeof(path)) || !h264.openMp4(&store.fs(), path)) {
    Serial.println("openMp4 FAILED");
    while (true) delay(1000);
  }

  Serial.printf("Recording ~%.1fs wall-clock -> %s%s\n", RECORD_MS / 1000.0f, path,
                have_mic ? " +mic AAC" : "");
  if (have_mic) Serial.println("Speak near the board mic...");

  uint32_t ok = 0, fail = 0;
  const uint32_t t0 = millis();

  while ((millis() - t0) < RECORD_MS) {
    drainMic();

    camera_fb_t *fb = cam.capture(200);
    if (!fb) {
      fail++;
      continue;
    }

    const uint8_t *rgb = fb->buf;
    uint16_t rw = fb->width;
    uint16_t rh = fb->height;

    if (fb->width != ENC_W || fb->height != ENC_H) {
      if (!ppa.scale(fb, scaled, scale_cap, ENC_W, ENC_H)) {
        cam.release(fb);
        fail++;
        continue;
      }
      rgb = scaled;
      rw = ENC_W;
      rh = ENC_H;
    }

    size_t n = h264.encodeToFile(rgb, rw, rh);
    cam.release(fb);
    drainMic();

    if (n) {
      ok++;
      if ((ok % 15) == 0) {
        float elapsed = (millis() - t0) / 1000.0f;
        Serial.printf("  frames=%u  ach_fps=%.1f  pcm=%u B  rms=%.2f\n", (unsigned)ok,
                      elapsed > 0.01f ? ok / elapsed : 0, (unsigned)mic.pcmRamBytes(),
                      have_mic ? mic.rms() : 0.0f);
      }
    } else {
      fail++;
    }
  }

  drainMic();
  if (have_mic) {
    mic.stopPcmFile();
    h264.setPcmRam(mic.pcmRam(), mic.pcmRamBytes(), (uint32_t)mic.sampleRate());
  }

  h264.closeFile();  // remux .mp4 + AAC, delete temp bitstream
  if (have_mic) mic.freePcmRam();

  float sec = (millis() - t0) / 1000.0f;
  Serial.printf("Done %s  frames=%u  fail=%u  wall=%.1fs  avg_fps=%.1f\n", h264.filePath(),
                (unsigned)ok, (unsigned)fail, sec, sec > 0.01f ? ok / sec : 0);
}

void loop() {
  dbg.poll();
  delay(1000);
}
