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
 *
 * Default input is RGB565 (existing sketches). YUV420/GRAY8 skip RGB: ISP I420
 * is packed to the HW O_UYY_E_VYY layout. YUV422 UYVY and YUYV are accepted by
 * the HW encoder with no colour convert. Planar I420 is SW-encoder only — not P4 HW.
 */

struct esp32p4_h264_cfg_t {
  uint16_t width;
  uint16_t height;
  uint8_t fps;  // encoder RC / GOP hint only — not used for file duration
  uint8_t gop;
  uint32_t bitrate;
  uint8_t qp_min;
  uint8_t qp_max;
  /** Hint for begin(); encode(fb) may switch. Default RGB565. */
  esp32p4_cam_pixformat_t input_format;
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
  uint8_t gop() const { return _cfg.gop; }
  uint32_t bitrate() const { return _cfg.bitrate; }
  uint32_t framesEncoded() const { return _frames; }
  const char *inputName() const;

  /** Runtime RC (Espressif CID equivalents). Applied while the encoder is open. */
  bool setBitrate(uint32_t bps);
  bool setGop(uint8_t gop);
  bool setFps(uint8_t fps);
  bool setQp(uint8_t qp_min, uint8_t qp_max);
  bool forceIdr();

  size_t encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap, int *frame_type = nullptr);
  size_t encode(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out, size_t out_cap,
                int *frame_type = nullptr);

  /**
   * Start recording → only .mp4 is kept after closeFile(). Path must end with .mp4.
   * Optional raw PCM path is encoded to AAC-LC and fused as `mp4a` on close (then deleted).
   * Accepts any mounted Arduino FS (SD_MMC, FFat, LittleFS, …).
   */
  bool openMp4(fs::FS *fs, const char *mp4_path, const char *pcm_path = nullptr,
               uint32_t pcm_rate_hz = 16000);
  bool openMp4(ESP32P4_Sd *sd, const char *mp4_path, const char *pcm_path = nullptr,
               uint32_t pcm_rate_hz = 16000);
  /** Prefer openMp4. Raw Annex-B only if path is not .mp4. */
  bool openFile(fs::FS *fs, const char *path);
  bool openFile(ESP32P4_Sd *sd, const char *path);
  size_t encodeToFile(const camera_fb_t *fb);
  size_t encodeToFile(const uint8_t *rgb565, uint16_t w, uint16_t h);
  void closeFile();
  /** In-memory PCM (PSRAM) fused as AAC on close. Caller owns the buffer until closeFile(). */
  void setPcmRam(const uint8_t *p, size_t n, uint32_t rate_hz = 16000) {
    _pcm_ram = p;
    _pcm_ram_sz = n;
    if (p && n && rate_hz) _pcm_rate = rate_hz;
  }
  bool fileOpen() const { return _file_open; }
  uint64_t fileBytes() const { return _file_bytes; }
  const char *filePath() const { return _file_path; }
  uint32_t recordElapsedMs() const;
  uint8_t muxProgress() const { return _mux_pct; }

 private:
  enum PicKind : uint8_t { PIC_NONE = 0, PIC_RGB565, PIC_YUV_PACKED, PIC_UYVY, PIC_YUYV };

  bool ensureBuffers();
  bool createEncoder(PicKind kind);
  bool ensureEncoder(PicKind kind);
  void dropEncoder();
  PicKind picKindFor(esp32p4_cam_pixformat_t fmt) const;
  bool convertRgb565ToHwYuv(const uint8_t *rgb565, uint16_t w, uint16_t h);
  bool convertI420ToHwYuv(const uint8_t *i420, uint16_t w, uint16_t h);
  bool convertGrayToHwYuv(const uint8_t *gray, uint16_t w, uint16_t h);
  bool convertUyvy(const uint8_t *uyvy, uint16_t w, uint16_t h);
  bool convertYuyv(const uint8_t *yuyv, uint16_t w, uint16_t h);
  size_t submitFrame(const uint8_t *buf, uint32_t len, uint8_t *out, size_t out_cap,
                     int *frame_type);
  size_t processFrame(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out, size_t out_cap,
                      int *frame_type);
  bool endsWithIgnoreCase(const char *s, const char *suffix) const;

  esp32p4_h264_cfg_t _cfg{};
  void *_enc = nullptr;
  void *_param = nullptr;
  uint8_t *_yuv = nullptr;
  uint8_t *_nal = nullptr;
  uint8_t *_rgb_copy = nullptr;
  uint32_t _yuv_cap = 0;
  uint32_t _nal_cap = 0;
  uint32_t _rgb_cap = 0;
  bool _opened = false;
  bool _use_rgb565 = false;
  PicKind _pic = PIC_NONE;
  uint32_t _frames = 0;
  uint32_t _pts = 0;

  fs::FS *_fs = nullptr;
  File _file;
  bool _file_open = false;
  bool _mp4_mode = false;
  uint64_t _file_bytes = 0;
  uint32_t _rec_t0_ms = 0;
  char _file_path[64] = "";  // final .mp4 path
  char _tmp_path[64] = "";   // temp work file (deleted on close)
  char _pcm_path[64] = "";   // optional raw PCM fused on close
  uint32_t _pcm_rate = 0;
  const uint8_t *_pcm_ram = nullptr;
  size_t _pcm_ram_sz = 0;
  volatile uint8_t _mux_pct = 0;
};
