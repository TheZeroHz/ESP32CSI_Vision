/**
 * 15_MicSdRecord — ES8311 mic → WAV on SD / flash.
 *
 * Mic + SD GPIOs: board_config.h (docs/Custom-Boards.md).
 * Not tied to one vendor. Waveshare Nano: set CFG_MIC_DIN 11, CFG_MIC_PA 53.
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "15_MicSdRecord"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_AUDIO | ESP32P4_DBG_SD
#endif

#include <ESP_I2S.h>
#include "es8311_m3.h"

static const int SAMPLE_RATE = 16000;
static const int CHUNK = 256;
static const uint32_t RECORD_MS = 10000;  // wall-clock capture length

ESP32P4_Sd sd;
ESP32P4_StoragePref store;
I2SClass i2s;

static bool nextWavPath(char *out, size_t out_cap) {
  if (!store.exists("/AUDIO") && !store.mkdir("/AUDIO")) return false;
  for (uint32_t i = 1; i < 100000; i++) {
    snprintf(out, out_cap, "/AUDIO/REC_%05lu.wav", (unsigned long)i);
    if (!store.exists(out)) return true;
  }
  return false;
}

static void writeWavHeader(File &f, uint32_t data_bytes, uint32_t sample_rate) {
  // Standard PCM WAV: mono, 16-bit
  const uint16_t channels = 1;
  const uint16_t bits = 16;
  const uint32_t byte_rate = sample_rate * channels * (bits / 8);
  const uint16_t block_align = channels * (bits / 8);
  const uint32_t riff_size = 36 + data_bytes;

  auto le16 = [&](uint16_t v) {
    f.write((uint8_t)(v & 0xff));
    f.write((uint8_t)((v >> 8) & 0xff));
  };
  auto le32 = [&](uint32_t v) {
    f.write((uint8_t)(v & 0xff));
    f.write((uint8_t)((v >> 8) & 0xff));
    f.write((uint8_t)((v >> 16) & 0xff));
    f.write((uint8_t)((v >> 24) & 0xff));
  };

  f.seek(0);
  f.write((const uint8_t *)"RIFF", 4);
  le32(riff_size);
  f.write((const uint8_t *)"WAVE", 4);
  f.write((const uint8_t *)"fmt ", 4);
  le32(16);  // PCM fmt chunk size
  le16(1);   // PCM
  le16(channels);
  le32(sample_rate);
  le32(byte_rate);
  le16(block_align);
  le16(bits);
  f.write((const uint8_t *)"data", 4);
  le32(data_bytes);
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== 15_MicSdRecord (ES8311 -> WAV) ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.printf("APP_STORAGE pref=%s\n", store.kindName(APP_STORAGE));
  esp32csi_print_sd_config(esp32csi_sd_config());
  esp32csi_print_mic_config(esp32csi_mic_config());

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("Storage FAILED - insert SD or use a flash partition");
    while (true) delay(1000);
  }
  Serial.printf("Storage %s  total=%llu KB\n", store.label(),
                (unsigned long long)(store.totalBytes() / 1024ULL));

  if (!es8311_i2s_begin(i2s, SAMPLE_RATE)) {
    while (true) delay(1000);
  }

  char path[48];
  if (!nextWavPath(path, sizeof(path))) {
    Serial.println("Could not allocate /AUDIO/REC_*.wav name");
    while (true) delay(1000);
  }

  File wav = store.fs().open(path, FILE_WRITE);
  if (!wav) {
    Serial.println("open WAV FAILED");
    while (true) delay(1000);
  }

  // Placeholder header; rewrite with final sizes after capture.
  writeWavHeader(wav, 0, SAMPLE_RATE);

  Serial.printf("Recording ~%.1fs -> %s\n", RECORD_MS / 1000.0f, path);
  Serial.println("Speak near the board mic...");

  int16_t buf[CHUNK];
  uint32_t data_bytes = 0;
  uint64_t energy = 0;
  uint32_t samples = 0;
  const uint32_t t0 = millis();
  uint32_t last_hb = t0;

  while ((millis() - t0) < RECORD_MS) {
    size_t got = i2s.readBytes((char *)buf, sizeof(buf));
    int n = (int)(got / sizeof(int16_t));
    if (n <= 0) {
      delay(1);
      continue;
    }

    size_t wr = wav.write((const uint8_t *)buf, (size_t)n * sizeof(int16_t));
    if (wr != (size_t)n * sizeof(int16_t)) {
      Serial.println("SD write short - stopping");
      break;
    }
    data_bytes += (uint32_t)wr;

    for (int i = 0; i < n; i++) {
      int32_t s = buf[i];
      energy += (uint64_t)(s * s);
      samples++;
    }

    if (millis() - last_hb >= 1000) {
      last_hb = millis();
      float rms = samples ? sqrtf((float)energy / (float)samples) : 0.f;
      energy = 0;
      samples = 0;
      Serial.printf("  t=%lus  wrote=%lu KB  rms~%.0f\n",
                    (unsigned long)((millis() - t0) / 1000UL),
                    (unsigned long)(data_bytes / 1024UL), rms);
    }
  }

  writeWavHeader(wav, data_bytes, SAMPLE_RATE);
  wav.flush();
  wav.close();

  Serial.printf("Done. %lu bytes PCM -> %s\n", (unsigned long)data_bytes, path);
  Serial.println("Copy the WAV off storage, or open it via 14_EthSdBrowser.");
}

void loop() {
  dbg.poll();
  delay(1000);
}
