#pragma once

/**
 * V4L2 M2M codec + ISP meta nodes (Espressif /dev/video10–12, /dev/video20).
 * Wraps existing ESP32P4_Jpeg / ESP32P4_H264 / ESP32P4_Isp — not IDF esp_video_init().
 *
 *   cam.begin(board);
 *   jpeg.begin(cam.width(), cam.height());
 *   h264.begin(cam.width(), cam.height());
 *   m2m.begin({&cam, &jpeg, &h264});   // registers video10/11/12/20
 *
 * USERPTR or v4l.mmap() MMAP queues. Do not mix with cam.capture() on the same FB pool.
 */

#include <stdint.h>

#include "cam/ESP32P4_Camera.h"
#include "h264/ESP32P4_H264.h"
#include "jpeg/ESP32P4_Jpeg.h"

class ESP32P4_Isp;

#if __has_include("linux/videodev2.h")
#include "linux/videodev2.h"
#define ESP32P4_V4L2M2M_HAS_V4L2 1
#else
#define ESP32P4_V4L2M2M_HAS_V4L2 0
#endif

#ifndef ESP32P4_V4L2_DEV_JPEG_ENC
#define ESP32P4_V4L2_DEV_JPEG_ENC "/dev/video10"
#endif
#ifndef ESP32P4_V4L2_DEV_H264_ENC
#define ESP32P4_V4L2_DEV_H264_ENC "/dev/video11"
#endif
#ifndef ESP32P4_V4L2_DEV_JPEG_DEC
#define ESP32P4_V4L2_DEV_JPEG_DEC "/dev/video12"
#endif
#ifndef ESP32P4_V4L2_DEV_ISP
#define ESP32P4_V4L2_DEV_ISP "/dev/video20"
#endif

class ESP32P4_V4l2M2m {
 public:
  struct Config {
    ESP32P4_Camera *cam = nullptr;
    ESP32P4_Jpeg *jpeg = nullptr;
    ESP32P4_H264 *h264 = nullptr;
    ESP32P4_Isp *isp = nullptr;
  };

  ~ESP32P4_V4l2M2m() { end(); }

  /** Register M2M/meta VFS nodes. Codecs must already be begin()'d when provided. */
  bool begin(const Config &cfg);
  void end();
  bool ready() const { return _cam != nullptr || _jpeg != nullptr || _h264 != nullptr; }

  int fdJpegEnc() const { return _fd_jpeg_enc; }
  int fdH264Enc() const { return _fd_h264_enc; }
  int fdJpegDec() const { return _fd_jpeg_dec; }
  int fdIspStats() const { return _fd_isp; }

  enum DevKind : uint8_t { DEV_JPEG_ENC = 0, DEV_H264_ENC, DEV_JPEG_DEC, DEV_ISP, DEV_N };

 private:
  friend struct ESP32P4_V4l2M2mVfs;

  struct DevSlot;

  bool registerDev(DevKind k);
  void unregisterDev(DevKind k);
  int devIoctl(DevKind k, unsigned long request, void *arg);
  int devOpen(DevKind k);
  int devClose(DevKind k);
  ssize_t devRead(DevKind k, void *dst, size_t n);
  ssize_t devWrite(DevKind k, const void *src, size_t n);
  int devFstat(DevKind k, void *st);

  bool tryEncode(DevSlot *d);
  bool fillIspStats(void *dst, size_t cap, size_t *out_len);

  ESP32P4_Camera *_cam = nullptr;
  ESP32P4_Jpeg *_jpeg = nullptr;
  ESP32P4_H264 *_h264 = nullptr;
  ESP32P4_Isp *_isp = nullptr;

  DevSlot *_slots[DEV_N]{};
  int _fd_jpeg_enc = -1;
  int _fd_h264_enc = -1;
  int _fd_jpeg_dec = -1;
  int _fd_isp = -1;
};
