/**
 * 16_MicSdWebFileManager - mic record + WebFileManager over Ethernet
 *
 * Board: Guition JC-ESP32P4-M3 (ES8311 mic, IP101 Ethernet, microSD)
 *
 * - Short GPIO1 to GND (header pin) to record 10 seconds of audio
 *   (SW1/BOOT is GPIO35 = Ethernet TXD1, so it cannot be used while ETH is up.
 *    ESP32-P4 has no capacitive "touch" GPIOs on this header.)
 * - Or type 'r' in Serial Monitor
 * - Files saved as /Recording/REC_00001.wav, REC_00002.wav, ...
 * - Browse / preview / download them in the web UI (audio preview supported)
 *
 * WebFileManager is bundled in ESP32CSI_Vision (no separate library).
 *
 * Serial @ 115200 · FAT32 SD · PSRAM on · Arduino-ESP32 3.x
 */

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 16384
#endif

#include <ESP32CSI_Vision.h>
#include <ESP_I2S.h>
#include "es8311_m3.h"

// Free header GPIO (see board pinout): short to GND to start record (active LOW).
// Do not use GPIO35/SW1 — that pad is RMII TXD1 once Ethernet starts.
static const int REC_BTN_PIN = 1;
static const int SAMPLE_RATE = 16000;
static const int CHUNK = 256;
static const uint32_t RECORD_MS = 10000;

ESP32P4_Sd sd;
WfmStorageFS sdVol(
    sd.fs(), "SD",
    []() -> uint64_t { return sd.totalBytes(); },
    []() -> uint64_t { return sd.usedBytes(); });

WfmNetwork net;
WebFileManager wfm(sdVol);
I2SClass i2s;

static volatile bool recording = false;
static bool btnWasDown = false;

static bool nextWavPath(char *out, size_t out_cap) {
  if (!sd.exists("/Recording") && !sd.mkdir("/Recording")) return false;
  for (uint32_t i = 1; i < 100000; i++) {
    snprintf(out, out_cap, "/Recording/REC_%05lu.wav", (unsigned long)i);
    if (!sd.exists(out)) return true;
  }
  return false;
}

static void writeWavHeader(File &f, uint32_t data_bytes, uint32_t sample_rate) {
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
  le32(16);
  le16(1);
  le16(channels);
  le32(sample_rate);
  le32(byte_rate);
  le16(block_align);
  le16(bits);
  f.write((const uint8_t *)"data", 4);
  le32(data_bytes);
}

static bool recordOneClip() {
  char path[48];
  if (!nextWavPath(path, sizeof(path))) {
    Serial.println("No free /Recording/REC_*.wav name");
    return false;
  }

  // Hold SD mutex so WebFileManager transfers do not collide mid-write.
  sdVol.lock();
  File wav = sd.fs().open(path, FILE_WRITE);
  if (!wav) {
    sdVol.unlock();
    Serial.println("open WAV FAILED");
    return false;
  }

  writeWavHeader(wav, 0, SAMPLE_RATE);
  Serial.printf("REC start -> %s (%.1fs). Speak now...\n", path, RECORD_MS / 1000.0f);

  int16_t buf[CHUNK];
  uint32_t data_bytes = 0;
  const uint32_t t0 = millis();
  uint32_t last_hb = t0;

  while ((millis() - t0) < RECORD_MS) {
    size_t got = i2s.readBytes((char *)buf, sizeof(buf));
    int n = (int)(got / sizeof(int16_t));
    if (n <= 0) {
      delay(1);
      continue;
    }
    size_t need = (size_t)n * sizeof(int16_t);
    size_t wr = wav.write((const uint8_t *)buf, need);
    if (wr != need) {
      Serial.println("SD write short - abort");
      break;
    }
    data_bytes += (uint32_t)wr;
    if (millis() - last_hb >= 1000) {
      last_hb = millis();
      Serial.printf("  recording... %lus  %lu KB\n",
                    (unsigned long)((millis() - t0) / 1000UL),
                    (unsigned long)(data_bytes / 1024UL));
    }
  }

  writeWavHeader(wav, data_bytes, SAMPLE_RATE);
  wav.flush();
  wav.close();
  sdVol.unlock();

  Serial.printf("REC done  %lu bytes -> %s\n", (unsigned long)data_bytes, path);
  Serial.println("Preview it in the browser under /SD/Recording/");
  wfm.refreshUsageAsync();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("=== 16_MicSdWebFileManager ===");

  pinMode(REC_BTN_PIN, INPUT_PULLUP);

  if (!sd.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("SD FAILED");
    while (true) delay(1000);
  }
  sdVol.begin();
  if (!sd.exists("/Recording")) sd.mkdir("/Recording");
  Serial.printf("SD %llu MB  (clips in /Recording)\n",
                (unsigned long long)(sd.cardSize() / (1024ULL * 1024ULL)));

  if (!es8311_i2s_begin(i2s, SAMPLE_RATE)) {
    while (true) delay(1000);
  }

  net.setHostname("csi-mic-archive");
  if (!net.beginEthernet(WfmNetwork::guitionM3Eth())) {
    Serial.println("ETH.begin FAILED");
    while (true) delay(1000);
  }
  Serial.println("Waiting for DHCP...");
  while (!net.ready()) delay(200);

  wfm.setName("Mic Archive").setPorts(80, 81);
  // wfm.setAuth("admin", "changeme");
  wfm.begin();
  wfm.startFileTask();

  Serial.printf("UI  http://%s/\n", net.localIP().toString().c_str());
  Serial.println("Record: short GPIO1 to GND, or type 'r' in Serial");
}

void loop() {
  if (!recording) wfm.loop();

  while (Serial.available()) {
    char c = (char)Serial.read();
    if ((c == 'r' || c == 'R') && !recording) {
      recording = true;
      Serial.println("Serial 'r' -> recording...");
      recordOneClip();
      recording = false;
    }
  }

  bool down = digitalRead(REC_BTN_PIN) == LOW;

  if (down && !btnWasDown && !recording) {
    Serial.println("GPIO1 LOW -> recording...");
    recording = true;
    recordOneClip();
    recording = false;
  }
  btnWasDown = down;
}
