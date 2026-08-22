#pragma once

/**
 * V4L2 POSIX + v4l2-ctl interop on top of ESP32P4_Camera.
 * Capture stays camera_fb_t — this does not replace begin()/capture().
 *
 *   cam.begin(board);
 *   v4l.begin(&cam);                 // registers /dev/video0 (CSI)
 *   v4l.ctl("--list-ctrls");
 *   ioctl(v4l.fd(), VIDIOC_S_CTRL, &c);
 *
 * Do not also start Arduino ESP_Video on the same CSI host.
 */

#include <stdint.h>
#include <sys/types.h>

#include "cam/ESP32P4_Camera.h"

#if __has_include("linux/videodev2.h")
#include "linux/videodev2.h"
#define ESP32P4_HAS_V4L2 1
#else
#define ESP32P4_HAS_V4L2 0
#endif

#if ESP32P4_HAS_V4L2
#if __has_include("esp_video_ioctl.h")
#include "esp_video_ioctl.h"
#endif
#endif

/** Device nodes match Espressif naming. Do not include esp_video_device.h —
 *  ESP_VIDEO_USB_UVC_DEVICE_NAME is a function-like macro there. */
#ifndef ESP32P4_V4L2_DEV_CSI
#define ESP32P4_V4L2_DEV_CSI "/dev/video0"
#endif
#ifndef ESP32P4_V4L2_DEV_DVP
#define ESP32P4_V4L2_DEV_DVP "/dev/video2"
#endif
#ifndef ESP32P4_V4L2_DEV_SPI
#define ESP32P4_V4L2_DEV_SPI "/dev/video3"
#endif
#ifndef ESP32P4_V4L2_DEV_SPI1
#define ESP32P4_V4L2_DEV_SPI1 "/dev/video4"
#endif
#ifndef ESP32P4_V4L2_DEV_UVC
#define ESP32P4_V4L2_DEV_UVC "/dev/video40"
#endif

class ESP32P4_V4l2 {
 public:
  ~ESP32P4_V4l2() { end(); }

  /** Register POSIX node (default path from cam.bus()) and bind to `cam`. */
  bool begin(ESP32P4_Camera *cam, const char *path = nullptr);
  void end();
  bool ready() const { return _cam != nullptr; }

  ESP32P4_Camera *camera() const { return _cam; }
  const char *path() const { return _path; }
  /** POSIX fd from open(path), or -1 if VFS is off. */
  int fd() const { return _fd; }

  /** Linux ioctl(request, arg). 0 = ok, -1 = errno. */
  int ioctl(unsigned long request, void *arg);

  bool setCtrl(uint32_t id, int32_t value);
  bool getCtrl(uint32_t id, int32_t *value) const;
  bool queryCtrl(uint32_t id, char *name, int32_t *minv, int32_t *maxv, int32_t *step,
                 int32_t *defv) const;

  /** Serial dump like `v4l2-ctl --list-ctrls` / `--list-formats`. */
  void listCtrls() const;
  void listFormats() const;

  /**
   * v4l2-ctl-style command line (no binary name required):
   *   --list-ctrls  --list-formats  --get-fmt-video  --all
   *   --set-ctrl brightness=0,gain=16
   *   --get-ctrl brightness,gain
   *   --set-fmt-video=pixelformat=RGBP
   *   --stream-count=1
   */
  int ctl(const char *cmdline);

  /** Same as capture()/release(); use these *or* cam.capture(), not both. */
  camera_fb_t *dqbuf(uint32_t timeout_ms = 0);
  void qbuf(camera_fb_t *fb);

  /**
   * V4L2 MEMORY_MMAP: QUERYBUF.m.offset → DMA FB (zero-copy).
   * POSIX mmap() is still ENOSYS on ESP-IDF VFS — use this instead.
   */
  void *mmap(size_t length, off_t offset);
  int munmap(void *addr, size_t length);

  static const char *defaultPath(esp32p4_cam_bus_t bus);
  static const char *defaultPath(ESP32P4_Camera *cam);
  static uint32_t pixToFourcc(esp32p4_cam_pixformat_t fmt);
  static bool fourccToPix(uint32_t fourcc, esp32p4_cam_pixformat_t *out);

 private:
  friend struct ESP32P4_V4l2Vfs;
  int ioctlUnlocked(unsigned long request, void *arg);
  bool fillFmt(void *fmt) const;
  bool applyFmt(void *fmt);
  int queryCtrlIoctl(void *qc);
  bool ctrlMeta(uint32_t id, const char **name, int32_t *minv, int32_t *maxv, int32_t *step,
                int32_t *defv, uint32_t *type) const;
  bool registerVfs();
  void unregisterVfs();
  int vfsOpen();
  int vfsClose(int local_fd);
  int vfsRead(int local_fd, void *dst, size_t size);
  int vfsIoctl(int local_fd, int cmd, void *arg);
  int vfsFstat(int local_fd, void *st);

  ESP32P4_Camera *_cam = nullptr;
  char _path[20]{};
  int _fd = -1;
  bool _vfs = false;
  uint32_t _dq_ms = 2000;
  uint32_t _seq = 0;
  uint32_t _req_n = 0;
  uint8_t _open_n = 0;
  int _owner = 0;
  camera_fb_t *_held[ESP32P4_CAM_FB_MAX]{};
};
