#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

/**
 * AAC-LC encode of 16-bit PCM (VisualOn vo-aacenc).
 * Output is raw AAC frames (no ADTS) for an MP4 `mp4a` track.
 */
bool esp32p4_pcm16_to_aac_lc(const int16_t *pcm, size_t pcm_samples, uint32_t rate_hz,
                             uint16_t channels, uint32_t bitrate_bps, std::vector<uint8_t> &aac,
                             std::vector<uint32_t> &frame_sizes);
