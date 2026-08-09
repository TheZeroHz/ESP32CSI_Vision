#include "audio/ESP32P4_Mic.h"

#include <Wire.h>
#include <math.h>
#include <string.h>

#ifndef ESP32P4_M3_I2C_SDA
#define ESP32P4_M3_I2C_SDA 7
#endif
#ifndef ESP32P4_M3_I2C_SCL
#define ESP32P4_M3_I2C_SCL 8
#endif
#ifndef ESP32P4_M3_ES8311_ADDR
#define ESP32P4_M3_ES8311_ADDR 0x18
#endif
#ifndef ESP32P4_M3_I2S_MCLK
#define ESP32P4_M3_I2S_MCLK 13
#endif
#ifndef ESP32P4_M3_I2S_BCLK
#define ESP32P4_M3_I2S_BCLK 12
#endif
#ifndef ESP32P4_M3_I2S_WS
#define ESP32P4_M3_I2S_WS 10
#endif
#ifndef ESP32P4_M3_I2S_DOUT
#define ESP32P4_M3_I2S_DOUT 9
#endif
#ifndef ESP32P4_M3_I2S_DIN
#define ESP32P4_M3_I2S_DIN 48
#endif
#ifndef ESP32P4_M3_ES8311_PA
#define ESP32P4_M3_ES8311_PA 11
#endif

static bool es_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ESP32P4_M3_ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool es_read(uint8_t reg, uint8_t *val) {
  Wire.beginTransmission(ESP32P4_M3_ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ESP32P4_M3_ES8311_ADDR, 1) != 1) return false;
  *val = (uint8_t)Wire.read();
  return true;
}

bool ESP32P4_Mic::applyGainHw() {
  // ES8311 ADC_VOLUME (0x17): 0x00=-95.5 dB … 0xBF=0 dB … 0xFF=+32 dB
  // Map UI 0–100 → ~-40 dB … +32 dB (0x70 … 0xFF).
  int g = _gain;
  if (g < 0) g = 0;
  if (g > 100) g = 100;
  uint8_t vol = (uint8_t)(0x70 + (g * (0xFF - 0x70)) / 100);
  // Soft gain 1…12 for waveform + PCM (keeps quiet board mics usable).
  _soft_gain = 1 + (g * 11) / 100;
  return es_write(0x17, vol);
}

bool ESP32P4_Mic::setGain(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  _gain = percent;
  if (!_ready) {
    _soft_gain = 1 + (_gain * 11) / 100;
    return true;
  }
  return applyGainHw();
}

bool ESP32P4_Mic::initCodec() {
  uint8_t id = 0;
  if (!es_read(0xFD, &id)) {
    Serial.println("Mic: ES8311 not on I2C 0x18");
    return false;
  }
  Serial.printf("Mic: ES8311 id=0x%02X\n", id);

  if (!es_write(0x00, 0x1F)) return false;
  delay(20);
  if (!es_write(0x00, 0x00)) return false;
  if (!es_write(0x00, 0x80)) return false;
  if (!es_write(0x01, 0x3F)) return false;
  if (!es_write(0x02, ((1 - 1) << 5) | (0 << 3))) return false;
  if (!es_write(0x03, (0 << 6) | 0x10)) return false;
  if (!es_write(0x04, 0x10)) return false;
  if (!es_write(0x05, ((1 - 1) << 4) | (1 - 1))) return false;
  if (!es_write(0x06, (4 - 1))) return false;
  if (!es_write(0x07, 0x00)) return false;
  if (!es_write(0x08, 0xFF)) return false;
  uint8_t r00;
  if (!es_read(0x00, &r00)) return false;
  r00 &= 0xBF;
  if (!es_write(0x00, r00)) return false;
  if (!es_write(0x09, (3 << 2))) return false;
  if (!es_write(0x0A, (3 << 2))) return false;
  if (!es_write(0x0D, 0x01)) return false;
  if (!es_write(0x0E, 0x02)) return false;
  if (!es_write(0x12, 0x00)) return false;
  if (!es_write(0x13, 0x10)) return false;
  if (!es_write(0x1C, 0x6A)) return false;
  if (!es_write(0x37, 0x08)) return false;
  // Analog MIC select + PGA +30 dB (PGAGAIN=10)
  if (!es_write(0x14, 0x1A)) return false;
  // ADC scale-up +24 dB (bits 2:0 = 4) — fine volume via 0x17 / setGain()
  if (!es_write(0x16, 0x04)) return false;
  if (!applyGainHw()) return false;
  return true;
}

bool ESP32P4_Mic::begin(int sample_rate) {
  end();
  _rate = sample_rate > 0 ? sample_rate : 16000;

  Wire.begin(ESP32P4_M3_I2C_SDA, ESP32P4_M3_I2C_SCL);
  Wire.setClock(100000);

  pinMode(ESP32P4_M3_ES8311_PA, OUTPUT);
  digitalWrite(ESP32P4_M3_ES8311_PA, LOW);  // speaker off — avoid howl into mic

  _i2s.setPins(ESP32P4_M3_I2S_BCLK, ESP32P4_M3_I2S_WS, ESP32P4_M3_I2S_DOUT, ESP32P4_M3_I2S_DIN,
               ESP32P4_M3_I2S_MCLK);
  if (!_i2s.begin(I2S_MODE_STD, _rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO,
                  I2S_STD_SLOT_LEFT)) {
    Serial.println("Mic: I2S begin failed");
    return false;
  }
  // Keep reads short so a mic drain loop cannot stall HTTP /record handlers.
  _i2s.setTimeout(10);
  if (!initCodec()) {
    Serial.println("Mic: ES8311 init failed");
    _i2s.end();
    return false;
  }
  memset(_wave, 0, sizeof(_wave));
  _rms = 0;
  _peak = 0;
  _ready = true;
  Serial.printf("Mic: ready @ %d Hz  gain=%d\n", _rate, _gain);
  return true;
}

void ESP32P4_Mic::end() {
  stopPcmFile();
  if (_ready) {
    _i2s.end();
    _ready = false;
  }
}

bool ESP32P4_Mic::startPcmFile(ESP32P4_Sd *sd, const char *path) {
  return startPcmFile((sd && sd->mounted()) ? &sd->fs() : nullptr, path);
}

bool ESP32P4_Mic::startPcmFile(fs::FS *fs, const char *path) {
  stopPcmFile();
  if (!_ready || !fs || !path || !path[0]) return false;
  _pcm = fs->open(path, FILE_WRITE);
  if (!_pcm) {
    Serial.printf("Mic: open PCM %s failed\n", path);
    return false;
  }
  _fs = fs;
  strncpy(_pcm_path, path, sizeof(_pcm_path) - 1);
  _pcm_path[sizeof(_pcm_path) - 1] = '\0';
  _pcm_bytes = 0;
  _pcm_open = true;
  Serial.printf("Mic: PCM capture -> %s\n", _pcm_path);
  return true;
}

void ESP32P4_Mic::stopPcmFile() {
  if (_pcm_open) {
    _pcm.flush();
    _pcm.close();
    _pcm_open = false;
    Serial.printf("Mic: PCM closed  bytes=%llu\n", (unsigned long long)_pcm_bytes);
  }
  _fs = nullptr;
}

void ESP32P4_Mic::poll() {
  if (!_ready) return;

  const int soft = _soft_gain > 0 ? _soft_gain : 1;

  int16_t buf[256];
  size_t got = _i2s.readBytes((char *)buf, sizeof(buf));
  int n = (int)(got / sizeof(int16_t));
  if (n <= 0) return;

  for (int i = 0; i < n; i++) {
    int32_t v = (int32_t)buf[i] * soft;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    buf[i] = (int16_t)v;
  }

  if (_pcm_open) {
    size_t wr = _pcm.write((const uint8_t *)buf, (size_t)n * sizeof(int16_t));
    _pcm_bytes += wr;
  }

  double acc = 0;
  int32_t pk = 0;
  const int bins = ESP32P4_MIC_WAVE_BINS;
  const int step = (n + bins - 1) / bins;
  for (int b = 0; b < bins; b++) {
    int i0 = b * step;
    if (i0 >= n) {
      _wave[b] = 0;
      continue;
    }
    int i1 = i0 + step;
    if (i1 > n) i1 = n;
    int32_t peak_bin = 0;
    for (int i = i0; i < i1; i++) {
      int16_t v = buf[i];
      int32_t a = v < 0 ? -(int32_t)v : (int32_t)v;
      if (a > peak_bin) peak_bin = a;
      if (a > pk) pk = a;
      acc += (double)v * (double)v;
    }
    int scaled = (int)((peak_bin * 100) / 32768);
    if (scaled > 100) scaled = 100;
    _wave[b] = (int8_t)scaled;
  }
  _peak = (float)pk / 32768.0f;
  if (_peak > 1.0f) _peak = 1.0f;
  _rms = (float)sqrt(acc / (double)n) / 32768.0f;
  if (_rms > 1.0f) _rms = 1.0f;
}

void ESP32P4_Mic::copyWave(int8_t *out, size_t n) const {
  if (!out || !n) return;
  size_t m = n < ESP32P4_MIC_WAVE_BINS ? n : ESP32P4_MIC_WAVE_BINS;
  memcpy(out, _wave, m);
  if (n > m) memset(out + m, 0, n - m);
}
