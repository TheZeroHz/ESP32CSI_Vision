#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>
#include <FS.h>
#include <stddef.h>
#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "sd/ESP32P4_Sd.h"
#include "freertos/FreeRTOS.h"

#ifndef ESP32P4_MIC_WAVE_BINS
#define ESP32P4_MIC_WAVE_BINS 64
#endif

enum esp32p4_mic_type_t : uint8_t {
  ESP32P4_MIC_ES8311 = 0,
  ESP32P4_MIC_CUSTOM = 255,
};

/** Mic config helper, similar to `esp32p4_cam_config_t` / `esp32p4_sd_config_t`. */
struct esp32p4_mic_config_t {
  esp32p4_mic_type_t type = ESP32P4_MIC_ES8311;
  int sample_rate = 16000;
  int i2c_sda = 7;
  int i2c_scl = 8;
  TwoWire *wire = nullptr;  // nullptr / &Wire = I2C0; &Wire1 = second bus
  uint8_t es8311_addr = 0x18;
  int i2s_mclk = 13;
  int i2s_bclk = 12;
  int i2s_ws = 10;
  int i2s_dout = 9;
  int i2s_din = 48;
  int pa_gpio = 11;
};

esp32p4_mic_config_t esp32p4_mic_config_default();
esp32p4_mic_config_t esp32p4_mic_config_board(esp32p4_board_t board);

/**
 * Mic capture helper: live UI waveform + optional PCM fused into MP4 recordings.
 * Supports ES8311 today, with board presets or a fully custom pin config.
 */
class ESP32P4_Mic {
 public:
  bool begin(esp32p4_board_t board);
  bool begin(const esp32p4_mic_config_t &cfg);
  bool begin(int sample_rate = 16000,
             esp32p4_board_t board = ESP32P4_BOARD_GUITION_M3);
  bool begin(int sample_rate, const esp32p4_mic_config_t &cfg);
  void end();
  bool ready() const { return _ready; }
  int sampleRate() const { return _rate; }
  esp32p4_mic_type_t type() const { return _cfg.type; }
  const esp32p4_mic_config_t &config() const { return _cfg; }

  /** Drain I2S: update levels; if a PCM file is open, append samples. Call often. */
  void poll();

  bool startPcmFile(fs::FS *fs, const char *path);
  bool startPcmFile(ESP32P4_Sd *sd, const char *path);
  /** Capture PCM into PSRAM (no SD DMA). Used while MJPEG/Wi-Fi is live. */
  bool startPcmRam(size_t cap_bytes = 0);
  void stopPcmFile();
  void freePcmRam();
  bool pcmFileOpen() const { return _pcm_open; }
  const char *pcmPath() const { return _pcm_path; }
  uint64_t pcmBytes() const { return _pcm_bytes; }
  const uint8_t *pcmRam() const { return _pcm_ram; }
  size_t pcmRamBytes() const { return _pcm_ram ? (size_t)_pcm_bytes : 0; }

  /**
   * Mic capture gain 0–100 (UI slider). Updates ES8311 ADC volume + soft gain
   * immediately; waveform and recorded PCM both follow.
   */
  bool setGain(int percent);
  int gain() const { return _gain; }

  float rms() const { return _rms; }
  float peak() const { return _peak; }
  void copyWave(int8_t *out, size_t n) const;
  size_t readStream(int16_t *out, size_t max_samples);
  void clearStream();

 private:
  bool initCodec();
  bool applyGainHw();
  bool esWrite(uint8_t reg, uint8_t val);
  bool esRead(uint8_t reg, uint8_t *val);
  TwoWire &bus() { return _cfg.wire ? *_cfg.wire : Wire; }

  I2SClass _i2s;
  fs::FS *_fs = nullptr;
  File _pcm;
  uint8_t *_pcm_ram = nullptr;
  size_t _pcm_ram_cap = 0;
  esp32p4_mic_config_t _cfg{};
  bool _ready = false;
  bool _pcm_open = false;
  int _rate = 16000;
  int _gain = 55;       // 0–100
  int _soft_gain = 4;   // linear multiplier applied in poll()
  uint64_t _pcm_bytes = 0;
  float _rms = 0;
  float _peak = 0;
  int8_t _wave[ESP32P4_MIC_WAVE_BINS]{};
  static constexpr size_t STREAM_RING_SAMPLES = 4096;
  int16_t _stream_ring[STREAM_RING_SAMPLES]{};
  volatile size_t _stream_r = 0;
  volatile size_t _stream_w = 0;
  portMUX_TYPE _stream_mux = portMUX_INITIALIZER_UNLOCKED;
  char _pcm_path[64] = "";
};
