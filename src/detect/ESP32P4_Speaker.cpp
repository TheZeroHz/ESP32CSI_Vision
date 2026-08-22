#include "detect/ESP32P4_Speaker.h"

#include <new>

#include "esp_log.h"
#include "esp_timer.h"
#include "speaker_verification.hpp"

static const char *TAG = "ESP32P4_Speaker";

bool ESP32P4_Speaker::begin(int seconds) {
  end();
#if (__has_include("dl_fbank.hpp") && __has_include("dl_audio_wav.hpp")) || defined(ESP32P4_ESPDL_ENABLE_SPEAKER)
  auto *sv = new (std::nothrow) SpeakerVerification(seconds);
  if (!sv) {
    ESP_LOGE(TAG, "SpeakerVerification alloc failed");
    return false;
  }
  _impl = sv;
  ESP_LOGI(TAG, "ready %ds dim=%d", seconds, sv->get_embedding_dim());
  return true;
#else
  (void)seconds;
  ESP_LOGW(TAG, "esp-dl audio (dl_fbank / dl_audio_wav) not vendored");
  return false;
#endif
}

void ESP32P4_Speaker::end() {
#if (__has_include("dl_fbank.hpp") && __has_include("dl_audio_wav.hpp")) || defined(ESP32P4_ESPDL_ENABLE_SPEAKER)
  if (_impl) {
    delete static_cast<SpeakerVerification *>(_impl);
    _impl = nullptr;
  }
#else
  _impl = nullptr;
#endif
  _last_ms = 0;
}

float *ESP32P4_Speaker::embedPcm(const int16_t *samples, size_t num_samples) {
#if (__has_include("dl_fbank.hpp") && __has_include("dl_audio_wav.hpp")) || defined(ESP32P4_ESPDL_ENABLE_SPEAKER)
  if (!_impl || !samples || num_samples == 0) return nullptr;
  const int64_t t0 = esp_timer_get_time();
  float *e = static_cast<SpeakerVerification *>(_impl)->run(samples, num_samples);
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  return e;
#else
  (void)samples;
  (void)num_samples;
  return nullptr;
#endif
}

float *ESP32P4_Speaker::embedWav(const uint8_t *wav, size_t len) {
#if (__has_include("dl_fbank.hpp") && __has_include("dl_audio_wav.hpp")) || defined(ESP32P4_ESPDL_ENABLE_SPEAKER)
  if (!_impl || !wav || len == 0) return nullptr;
  const int64_t t0 = esp_timer_get_time();
  float *e = static_cast<SpeakerVerification *>(_impl)->run(wav, len);
  _last_ms = (int)((esp_timer_get_time() - t0) / 1000);
  return e;
#else
  (void)wav;
  (void)len;
  return nullptr;
#endif
}

float ESP32P4_Speaker::similarity(const float *a, const float *b) {
#if (__has_include("dl_fbank.hpp") && __has_include("dl_audio_wav.hpp")) || defined(ESP32P4_ESPDL_ENABLE_SPEAKER)
  if (!_impl || !a || !b) return 0.f;
  return static_cast<SpeakerVerification *>(_impl)->compute_similarity(a, b);
#else
  (void)a;
  (void)b;
  return 0.f;
#endif
}

int ESP32P4_Speaker::dim() const {
#if (__has_include("dl_fbank.hpp") && __has_include("dl_audio_wav.hpp")) || defined(ESP32P4_ESPDL_ENABLE_SPEAKER)
  if (!_impl) return 0;
  return static_cast<SpeakerVerification *>(_impl)->get_embedding_dim();
#else
  return 0;
#endif
}
