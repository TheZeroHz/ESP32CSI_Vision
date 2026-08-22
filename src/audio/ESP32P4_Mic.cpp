#include "audio/ESP32P4_Mic.h"

#include <Wire.h>
#include <math.h>
#include <string.h>

#include "mem/ESP32P4_Psram.h"

esp32p4_mic_config_t esp32p4_mic_config_default() {
  return esp32p4_mic_config_board(ESP32P4_BOARD_GUITION_M3);
}

esp32p4_mic_config_t esp32p4_mic_config_board(esp32p4_board_t board) {
  esp32p4_mic_config_t c{};
  switch (board) {
    case ESP32P4_BOARD_WAVESHARE_NANO:
      // Waveshare ESP32-P4-Nano: ES8311 DIN=GPIO11, PA=GPIO53 (speaker amp off for mic).
      c.i2s_din = 11;
      c.pa_gpio = 53;
      break;
    case ESP32P4_BOARD_GUITION_M3:
    case ESP32P4_BOARD_FUNCTION_EV:
    case ESP32P4_BOARD_CUSTOM:
    default:
      break;
  }
  return c;
}

bool ESP32P4_Mic::esWrite(uint8_t reg, uint8_t val) {
  TwoWire &i2c = bus();
  i2c.beginTransmission(_cfg.es8311_addr);
  i2c.write(reg);
  i2c.write(val);
  return i2c.endTransmission() == 0;
}

bool ESP32P4_Mic::esRead(uint8_t reg, uint8_t *val) {
  TwoWire &i2c = bus();
  i2c.beginTransmission(_cfg.es8311_addr);
  i2c.write(reg);
  if (i2c.endTransmission(false) != 0) return false;
  if (i2c.requestFrom((int)_cfg.es8311_addr, 1) != 1) return false;
  *val = (uint8_t)i2c.read();
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
  return esWrite(0x17, vol);
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
  if (_cfg.type != ESP32P4_MIC_ES8311 && _cfg.type != ESP32P4_MIC_CUSTOM) {
    Serial.printf("Mic: unsupported type=%u\n", (unsigned)_cfg.type);
    return false;
  }
  uint8_t id = 0;
  if (!esRead(0xFD, &id)) {
    Serial.println("Mic: ES8311 not on I2C 0x18");
    return false;
  }
  Serial.printf("Mic: ES8311 id=0x%02X\n", id);

  if (!esWrite(0x00, 0x1F)) return false;
  delay(20);
  if (!esWrite(0x00, 0x00)) return false;
  if (!esWrite(0x00, 0x80)) return false;
  if (!esWrite(0x01, 0x3F)) return false;
  if (!esWrite(0x02, ((1 - 1) << 5) | (0 << 3))) return false;
  if (!esWrite(0x03, (0 << 6) | 0x10)) return false;
  if (!esWrite(0x04, 0x10)) return false;
  if (!esWrite(0x05, ((1 - 1) << 4) | (1 - 1))) return false;
  if (!esWrite(0x06, (4 - 1))) return false;
  if (!esWrite(0x07, 0x00)) return false;
  if (!esWrite(0x08, 0xFF)) return false;
  uint8_t r00;
  if (!esRead(0x00, &r00)) return false;
  r00 &= 0xBF;
  if (!esWrite(0x00, r00)) return false;
  if (!esWrite(0x09, (3 << 2))) return false;
  if (!esWrite(0x0A, (3 << 2))) return false;
  if (!esWrite(0x0D, 0x01)) return false;
  if (!esWrite(0x0E, 0x02)) return false;
  if (!esWrite(0x12, 0x00)) return false;
  if (!esWrite(0x13, 0x10)) return false;
  if (!esWrite(0x1C, 0x6A)) return false;
  if (!esWrite(0x37, 0x08)) return false;
  // Analog MIC select + PGA +30 dB (PGAGAIN=10)
  if (!esWrite(0x14, 0x1A)) return false;
  // ADC scale-up +24 dB (bits 2:0 = 4) — fine volume via 0x17 / setGain()
  if (!esWrite(0x16, 0x04)) return false;
  if (!applyGainHw()) return false;
  return true;
}

bool ESP32P4_Mic::begin(esp32p4_board_t board) { return begin(esp32p4_mic_config_board(board)); }

bool ESP32P4_Mic::begin(const esp32p4_mic_config_t &cfg) {
  return begin(cfg.sample_rate, cfg);
}

bool ESP32P4_Mic::begin(int sample_rate, esp32p4_board_t board) {
  return begin(sample_rate, esp32p4_mic_config_board(board));
}

bool ESP32P4_Mic::begin(int sample_rate, const esp32p4_mic_config_t &cfg) {
  end();
  _cfg = cfg;
  _rate = sample_rate > 0 ? sample_rate : (_cfg.sample_rate > 0 ? _cfg.sample_rate : 16000);
  _cfg.sample_rate = _rate;

  TwoWire &i2c = bus();
  i2c.begin(_cfg.i2c_sda, _cfg.i2c_scl);
  i2c.setClock(100000);

  if (_cfg.pa_gpio >= 0) {
    pinMode(_cfg.pa_gpio, OUTPUT);
    digitalWrite(_cfg.pa_gpio, LOW);  // speaker off — avoid howl into mic
  }

  _i2s.setPins(_cfg.i2s_bclk, _cfg.i2s_ws, _cfg.i2s_dout, _cfg.i2s_din, _cfg.i2s_mclk);
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
  clearStream();
  _ready = true;
  Serial.printf("Mic: ready type=%u @ %d Hz  gain=%d  i2c=%s\n", (unsigned)_cfg.type, _rate, _gain,
                (&bus() == &Wire) ? "Wire" : "Wire1");
  return true;
}

void ESP32P4_Mic::end() {
  stopPcmFile();
  freePcmRam();
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
  freePcmRam();
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

bool ESP32P4_Mic::startPcmRam(size_t cap_bytes) {
  stopPcmFile();
  freePcmRam();
  if (!_ready) return false;
  if (cap_bytes < 16000 * 2 * 30) cap_bytes = 16000 * 2 * 180;  // 3 min @ 16 kHz mono
  if (cap_bytes > 8 * 1024 * 1024) cap_bytes = 8 * 1024 * 1024;
  _pcm_ram = (uint8_t *)esp32p4_psram_alloc(cap_bytes);
  if (!_pcm_ram) {
    Serial.println("Mic: PCM PSRAM alloc failed");
    return false;
  }
  _pcm_ram_cap = cap_bytes;
  _pcm_bytes = 0;
  _pcm_path[0] = '\0';
  _pcm_open = true;
  Serial.printf("Mic: PCM capture -> PSRAM  cap=%u\n", (unsigned)cap_bytes);
  return true;
}

void ESP32P4_Mic::freePcmRam() {
  if (_pcm_ram) {
    esp32p4_psram_free(_pcm_ram);
    _pcm_ram = nullptr;
  }
  _pcm_ram_cap = 0;
}

void ESP32P4_Mic::stopPcmFile() {
  if (_pcm_open) {
    if (_pcm) {
      _pcm.flush();
      _pcm.close();
    }
    _pcm_open = false;
    Serial.printf("Mic: PCM closed  bytes=%llu%s\n", (unsigned long long)_pcm_bytes,
                  _pcm_ram ? " (PSRAM)" : "");
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

  portENTER_CRITICAL(&_stream_mux);
  for (int i = 0; i < n; i++) {
    size_t next = (_stream_w + 1) % STREAM_RING_SAMPLES;
    if (next == _stream_r) _stream_r = (_stream_r + 1) % STREAM_RING_SAMPLES;
    _stream_ring[_stream_w] = buf[i];
    _stream_w = next;
  }
  portEXIT_CRITICAL(&_stream_mux);

  if (_pcm_open) {
    const size_t nbytes = (size_t)n * sizeof(int16_t);
    if (_pcm_ram && _pcm_ram_cap) {
      size_t room = (_pcm_bytes < _pcm_ram_cap) ? (_pcm_ram_cap - (size_t)_pcm_bytes) : 0;
      size_t wr = nbytes < room ? nbytes : room;
      if (wr) memcpy(_pcm_ram + (size_t)_pcm_bytes, buf, wr);
      _pcm_bytes += wr;
    } else if (_pcm) {
      size_t wr = _pcm.write((const uint8_t *)buf, nbytes);
      _pcm_bytes += wr;
    }
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

size_t ESP32P4_Mic::readStream(int16_t *out, size_t max_samples) {
  if (!out || !max_samples) return 0;
  size_t n = 0;
  portENTER_CRITICAL(&_stream_mux);
  while (n < max_samples && _stream_r != _stream_w) {
    out[n++] = _stream_ring[_stream_r];
    _stream_r = (_stream_r + 1) % STREAM_RING_SAMPLES;
  }
  portEXIT_CRITICAL(&_stream_mux);
  return n;
}

void ESP32P4_Mic::clearStream() {
  portENTER_CRITICAL(&_stream_mux);
  _stream_r = 0;
  _stream_w = 0;
  portEXIT_CRITICAL(&_stream_mux);
}
