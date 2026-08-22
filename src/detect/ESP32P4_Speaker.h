#pragma once

/**
 * ESP-DL speaker verification (x-vector / TDNN).
 * Needs esp-dl audio (`dl_fbank` / `dl_audio_wav`) or ESP32P4_ESPDL_ENABLE_SPEAKER.
 * Weights: sv_tdnn_tiny_3s.espdl / sv_tdnn_tiny_6s.espdl
 */

#include <stddef.h>
#include <stdint.h>

class ESP32P4_Speaker {
 public:
  ESP32P4_Speaker() = default;
  ~ESP32P4_Speaker() { end(); }

  /** @param seconds 3 or 6 (other values fall back to 6). */
  bool begin(int seconds = 6);
  void end();
  bool ready() const { return _impl != nullptr; }

  /** 16 kHz mono int16 PCM. Caller must free() the returned embedding. */
  float *embedPcm(const int16_t *samples, size_t num_samples);
  /** WAV bytes. Caller must free() the returned embedding. */
  float *embedWav(const uint8_t *wav, size_t len);

  float similarity(const float *a, const float *b);
  int dim() const;
  int lastMs() const { return _last_ms; }

 private:
  void *_impl = nullptr;
  int _last_ms = 0;
};
