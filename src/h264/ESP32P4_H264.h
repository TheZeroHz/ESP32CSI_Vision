#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <FS.h>

#include "cam/ESP32P4_Camera.h"
#include "sd/ESP32P4_Sd.h"

/**
 * ESP32-P4 hardware H.264 encoder (Baseline).
 * Prefer openMp4() — encodes as fast as the pipeline allows, remuxes to .mp4
 * using wall-clock duration (no fixed fps / fixed clip length). Temp bitstream
 * is deleted; only the .mp4 remains.
 */

struct esp32p4_h264_cfg_t {
  uint16_t width;
  uint16_t height;
  uint8_t fps;  // encoder RC / GOP hint only — not used for file duration
  uint8_t gop;
  uint32_t bitrate;
  uint8_t qp_min;
  uint8_t qp_max;
};

esp32p4_h264_cfg_t esp32p4_h264_cfg_default(uint16_t w, uint16_t h);

class ESP32P4_H264 {
 public:
  bool begin(uint16_t w, uint16_t h, uint8_t fps = 15, uint32_t bitrate = 0);
  bool begin(const esp32p4_h264_cfg_t &cfg);
  void end();

  bool ready() const { return _enc != nullptr && _opened; }
  bool usesRgb565() const { return _use_rgb565; }
  uint16_t width() const { return _cfg.width; }
  uint16_t height() const { return _cfg.height; }
  uint8_t fps() const { return _cfg.fps; }
  uint32_t bitrate() const { return _cfg.bitrate; }
  uint32_t framesEncoded() const { return _frames; }

  size_t encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap, int *frame_type = nullptr);
  size_t encode(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out, size_t out_cap,
                int *frame_type = nullptr);

  /**
   * Start recording → only .mp4 is kept after closeFile(). Path must end with .mp4.
   * Optional raw PCM path is fused as a `sowt` audio track on close (then deleted).
   */
  bool openMp4(ESP32P4_Sd *sd, const char *mp4_path, const char *pcm_path = nullptr,
               uint32_t pcm_rate_hz = 16000);
  /** Prefer openMp4. Raw Annex-B only if path is not .mp4. */
  bool openFile(ESP32P4_Sd *sd, const char *path);
  size_t encodeToFile(const camera_fb_t *fb);
  size_t encodeToFile(const uint8_t *rgb565, uint16_t w, uint16_t h);
  void closeFile();
  bool fileOpen() const { return _file_open; }
  uint64_t fileBytes() const { return _file_bytes; }
  const char *filePath() const { return _file_path; }
  uint32_t recordElapsedMs() const;

 private:
  bool ensureBuffers();
  bool createEncoder(bool rgb565_le);
  bool convertRgb565ToHwYuv(const uint8_t *rgb565, uint16_t w, uint16_t h);
  size_t processFrame(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out, size_t out_cap,
                      int *frame_type);
  bool endsWithIgnoreCase(const char *s, const char *suffix) const;

  esp32p4_h264_cfg_t _cfg{};
  void *_enc = nullptr;
  uint8_t *_yuv = nullptr;
  uint8_t *_nal = nullptr;
  uint8_t *_rgb_copy = nullptr;
  uint32_t _yuv_cap = 0;
  uint32_t _nal_cap = 0;
  uint32_t _rgb_cap = 0;
  bool _opened = false;
  bool _use_rgb565 = false;
  uint32_t _frames = 0;
  uint32_t _pts = 0;

  ESP32P4_Sd *_sd = nullptr;
  File _file;
  bool _file_open = false;
  bool _mp4_mode = false;
  uint64_t _file_bytes = 0;
  uint32_t _rec_t0_ms = 0;
  char _file_path[64] = "";  // final .mp4 path
  char _tmp_path[64] = "";   // temp work file (deleted on close)
  char _pcm_path[64] = "";   // optional raw PCM fused on close
  uint32_t _pcm_rate = 0;
};
