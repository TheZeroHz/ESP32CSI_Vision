#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Remux Annex-B elementary stream → .mp4.
 * duration_ms = wall-clock recording length. Timing = duration_ms / frame_count
 * so player length matches real record time (no fixed fps).
 *
 * Optional raw PCM (16-bit LE mono/stereo) is encoded to AAC-LC and fused as `mp4a`.
 */
bool esp32p4_h264_annexb_to_mp4(fs::FS &fs, const char *annexb_path, const char *mp4_path,
                                uint16_t width, uint16_t height, uint32_t duration_ms);

bool esp32p4_h264_annexb_to_mp4(fs::FS &fs, const char *annexb_path, const char *mp4_path,
                                uint16_t width, uint16_t height, uint32_t duration_ms,
                                const char *pcm_path, uint32_t pcm_rate_hz, uint16_t pcm_channels,
                                const uint8_t *pcm_ram = nullptr, size_t pcm_ram_sz = 0,
                                volatile uint8_t *progress = nullptr);
