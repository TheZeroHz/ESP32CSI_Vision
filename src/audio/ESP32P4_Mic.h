#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>
#include <stddef.h>
#include <stdint.h>

#include "sd/ESP32P4_Sd.h"

#ifndef ESP32P4_MIC_WAVE_BINS
#define ESP32P4_MIC_WAVE_BINS 64
#endif

/**
 * Guition JC-ESP32P4-M3 onboard ES8311 mic (I2C 7/8, I2S 13/12/10/9/48, PA=11).
 * Provides live levels for UI waveform + optional raw PCM file while recording video.
 */
class ESP32P4_Mic {
 public:
  bool begin(int sample_rate = 16000);
  void end();
  bool ready() const { return _ready; }
  int sampleRate() const { return _rate; }

  /** Drain I2S: update levels; if a PCM file is open, append samples. Call often. */
  void poll();

  bool startPcmFile(ESP32P4_Sd *sd, const char *path);
  void stopPcmFile();
  bool pcmFileOpen() const { return _pcm_open; }
  const char *pcmPath() const { return _pcm_path; }
  uint64_t pcmBytes() const { return _pcm_bytes; }

  /**
   * Mic capture gain 0–100 (UI slider). Updates ES8311 ADC volume + soft gain
   * immediately; waveform and recorded PCM both follow.
   */
  bool setGain(int percent);
  int gain() const { return _gain; }

  float rms() const { return _rms; }
  float peak() const { return _peak; }
  void copyWave(int8_t *out, size_t n) const;

 private:
  bool initCodec();
  bool applyGainHw();

  I2SClass _i2s;
  ESP32P4_Sd *_sd = nullptr;
  File _pcm;
  bool _ready = false;
  bool _pcm_open = false;
  int _rate = 16000;
  int _gain = 55;       // 0–100
  int _soft_gain = 4;   // linear multiplier applied in poll()
  uint64_t _pcm_bytes = 0;
  float _rms = 0;
  float _peak = 0;
  int8_t _wave[ESP32P4_MIC_WAVE_BINS]{};
  char _pcm_path[64] = "";
};
