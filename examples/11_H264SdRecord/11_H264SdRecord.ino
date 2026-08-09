/**
 * 11_H264SdRecord — CSI → HW H.264 → .mp4 on preferred storage (no leftover .h264)
 *
 * Encodes as fast as the pipeline allows (no fixed fps pacing).
 * Stop after wall-clock RECORD_MS; MP4 duration matches real time.
 *
 * APP_STORAGE: AUTO / SD / FFAT / LITTLEFS / SPIFFS (large clips prefer SD).
 *
 * Serial @ 115200 · PSRAM on
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <ESP32CSI_Vision.h>

static const uint32_t RECORD_MS = 10000;  // wall-clock; press nothing — auto-stop
static const uint32_t RECORD_BITRATE = 1500000;
static const uint16_t ENC_W = 640;
static const uint16_t ENC_H = 480;

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_Ppa ppa;
ESP32P4_H264 h264;
uint8_t *scaled = nullptr;

static bool nextMp4Path(char *out, size_t out_cap) {
  if (!store.exists("/VIDEO") && !store.mkdir("/VIDEO")) return false;
  for (uint32_t i = 1; i < 100000; i++) {
    snprintf(out, out_cap, "/VIDEO/VID_%05lu.mp4", (unsigned long)i);
    if (!store.exists(out)) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 11_H264SdRecord (MP4 only, wall-clock) ===");
  Serial.printf("APP_STORAGE pref=%s\n", ESP32P4_StoragePref::kindName(APP_STORAGE));

  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }
  if (!store.begin(APP_STORAGE, false, &sd, ESP32P4_BOARD_GUITION_M3)) {
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

  // fps arg is encoder RC hint only — file duration uses wall clock
  if (!h264.begin(ENC_W, ENC_H, 30, RECORD_BITRATE)) {
    Serial.println("H264 begin FAILED");
    while (true) delay(1000);
  }

  char path[48];
  if (!nextMp4Path(path, sizeof(path)) || !h264.openMp4(&store.fs(), path)) {
    Serial.println("openMp4 FAILED");
    while (true) delay(1000);
  }

  Serial.printf("Recording ~%.1fs wall-clock (max speed) → %s\n", RECORD_MS / 1000.0f, path);

  uint32_t ok = 0, fail = 0;
  const uint32_t t0 = millis();

  while ((millis() - t0) < RECORD_MS) {
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

    if (n) {
      ok++;
      if ((ok % 15) == 0) {
        float elapsed = (millis() - t0) / 1000.0f;
        Serial.printf("  frames=%u  ach_fps=%.1f\n", (unsigned)ok,
                      elapsed > 0.01f ? ok / elapsed : 0);
      }
    } else {
      fail++;
    }
  }

  h264.closeFile();  // remux .mp4 with wall duration, delete temp
  float sec = (millis() - t0) / 1000.0f;
  Serial.printf("Done %s  frames=%u  fail=%u  wall=%.1fs  avg_fps=%.1f\n", h264.filePath(),
                (unsigned)ok, (unsigned)fail, sec, sec > 0.01f ? ok / sec : 0);
}

void loop() { delay(1000); }
