/**
 * 15_MicSdRecord - record Guition JC-ESP32P4-M3 mic to WAV on preferred storage
 *
 * Codec: onboard ES8311 (I2C 0x18) + I2S mic path
 * Pin map / init adapted from esp32-ai esp32p4/mictest.
 *
 * Writes 16-bit mono PCM WAV @ 16 kHz to /AUDIO/REC_xxxxx.wav
 * Speaks nothing (PA off) to reduce howl while capturing.
 *
 * APP_STORAGE: AUTO / SD / FFAT / LITTLEFS / SPIFFS
 *
 * Serial @ 115200 · PSRAM recommended
 * Board: Guition JC-ESP32P4-M3
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include <ESP32CSI_Vision.h>
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

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("=== 15_MicSdRecord (ES8311 -> WAV) ===");
  Serial.printf("APP_STORAGE pref=%s\n", ESP32P4_StoragePref::kindName(APP_STORAGE));

  if (!store.begin(APP_STORAGE, false, &sd, ESP32P4_BOARD_GUITION_M3)) {
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
  delay(1000);
}
