#include "v4l2/ESP32P4_V4l2M2m.h"

#include <stdarg.h>
#include <Arduino.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cam/ESP32P4_Isp.h"
#include "v4l2/ESP32P4_V4l2.h"

#if ESP32P4_V4L2M2M_HAS_V4L2
#include "esp_vfs.h"
#if __has_include("esp_video_ioctl.h")
#include "esp_video_ioctl.h"
#endif
#if __has_include("esp_video_isp_ioctl.h")
#include "esp_video_isp_ioctl.h"
#endif

#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264 v4l2_fourcc('H', '2', '6', '4')
#endif
#ifndef V4L2_BUF_CAP_SUPPORTS_USERPTR
#define V4L2_BUF_CAP_SUPPORTS_USERPTR (1 << 1)
#endif
#ifndef V4L2_BUF_CAP_SUPPORTS_MMAP
#define V4L2_BUF_CAP_SUPPORTS_MMAP (1 << 0)
#endif
#ifndef V4L2_META_FMT_ESP_ISP_STATS
#define V4L2_META_FMT_ESP_ISP_STATS v4l2_fourcc('E', 'S', 'T', 'A')
#endif

struct ESP32P4_V4l2M2m::DevSlot {
  ESP32P4_V4l2M2m *owner = nullptr;
  DevKind kind = DEV_JPEG_ENC;
  const char *path = nullptr;
  bool registered = false;
  uint8_t open_n = 0;

  uint16_t out_w = 640;
  uint16_t out_h = 480;
  uint32_t out_fourcc = V4L2_PIX_FMT_RGB565;
  uint16_t cap_w = 640;
  uint16_t cap_h = 480;
  uint32_t cap_fourcc = V4L2_PIX_FMT_JPEG;

  bool out_streaming = false;
  bool cap_streaming = false;
  bool out_queued = false;
  bool cap_queued = false;
  uint8_t *out_ptr = nullptr;
  size_t out_len = 0;
  uint8_t *cap_ptr = nullptr;
  size_t cap_cap = 0;
  size_t cap_len = 0;
  bool cap_ready = false;
  uint32_t seq = 0;
  int owner_pid = 0;

  uint8_t *scratch = nullptr;
  size_t scratch_cap = 0;
};

struct ESP32P4_V4l2M2mVfs {
  static int open_p(void *ctx, const char *, int, int) {
    auto *d = (ESP32P4_V4l2M2m::DevSlot *)ctx;
    return d && d->owner ? d->owner->devOpen(d->kind) : -1;
  }
  static int close_p(void *ctx, int) {
    auto *d = (ESP32P4_V4l2M2m::DevSlot *)ctx;
    return d && d->owner ? d->owner->devClose(d->kind) : -1;
  }
  static ssize_t read_p(void *ctx, int, void *dst, size_t n) {
    auto *d = (ESP32P4_V4l2M2m::DevSlot *)ctx;
    return d && d->owner ? d->owner->devRead(d->kind, dst, n) : -1;
  }
  static ssize_t write_p(void *ctx, int, const void *src, size_t n) {
    auto *d = (ESP32P4_V4l2M2m::DevSlot *)ctx;
    return d && d->owner ? d->owner->devWrite(d->kind, src, n) : -1;
  }
  static int ioctl_p(void *ctx, int, int cmd, va_list args) {
    auto *d = (ESP32P4_V4l2M2m::DevSlot *)ctx;
    void *arg = va_arg(args, void *);
    return d && d->owner ? d->owner->devIoctl(d->kind, (unsigned long)cmd, arg) : -1;
  }
  static int fstat_p(void *ctx, int, struct stat *st) {
    auto *d = (ESP32P4_V4l2M2m::DevSlot *)ctx;
    return d && d->owner ? d->owner->devFstat(d->kind, st) : -1;
  }
};

static int fail(int err) {
  errno = err;
  return -1;
}

static bool pix_from_fourcc(uint32_t fcc, esp32p4_cam_pixformat_t *out) {
  return ESP32P4_V4l2::fourccToPix(fcc, out);
}

static uint32_t fourcc_for_pix(esp32p4_cam_pixformat_t pix) {
  return ESP32P4_V4l2::pixToFourcc(pix);
}

static size_t frame_bytes(uint32_t fcc, uint16_t w, uint16_t h) {
  esp32p4_cam_pixformat_t pix = ESP32P4_PIXFORMAT_RGB565;
  if (!pix_from_fourcc(fcc, &pix)) return (size_t)w * h * 2;
  return esp32p4_pixformat_fb_bytes(pix, w, h);
}

static const char *dev_card(ESP32P4_V4l2M2m::DevKind k) {
  switch (k) {
    case ESP32P4_V4l2M2m::DEV_JPEG_ENC:
      return "CSI Vision JPEG enc";
    case ESP32P4_V4l2M2m::DEV_H264_ENC:
      return "CSI Vision H264 enc";
    case ESP32P4_V4l2M2m::DEV_JPEG_DEC:
      return "CSI Vision JPEG dec";
    case ESP32P4_V4l2M2m::DEV_ISP:
      return "CSI Vision ISP stats";
    default:
      return "CSI Vision M2M";
  }
}

static uint32_t v4l2_buf_type_for(ESP32P4_V4l2M2m::DevKind k, bool capture) {
  if (k == ESP32P4_V4l2M2m::DEV_ISP) return V4L2_BUF_TYPE_META_CAPTURE;
  return capture ? V4L2_BUF_TYPE_VIDEO_CAPTURE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

static bool is_m2m(ESP32P4_V4l2M2m::DevKind k) { return k != ESP32P4_V4l2M2m::DEV_ISP; }

bool ESP32P4_V4l2M2m::begin(const Config &cfg) {
  end();
  _cam = cfg.cam;
  _jpeg = cfg.jpeg;
  _h264 = cfg.h264;
  _isp = cfg.isp ? cfg.isp : (_cam && _cam->isp() ? _cam->isp() : nullptr);

  uint16_t w = _cam ? _cam->width() : (_h264 ? _h264->width() : 640);
  uint16_t h = _cam ? _cam->height() : (_h264 ? _h264->height() : 480);
  if (_jpeg && _cam) {
    w = _cam->width();
    h = _cam->height();
  }

  for (int i = 0; i < DEV_N; i++) {
    _slots[i] = new DevSlot();
    _slots[i]->owner = this;
    _slots[i]->kind = (DevKind)i;
    _slots[i]->out_w = w;
    _slots[i]->out_h = h;
    _slots[i]->cap_w = w;
    _slots[i]->cap_h = h;
    switch ((DevKind)i) {
      case DEV_JPEG_ENC:
        _slots[i]->path = ESP32P4_V4L2_DEV_JPEG_ENC;
        _slots[i]->out_fourcc = V4L2_PIX_FMT_RGB565;
        _slots[i]->cap_fourcc = V4L2_PIX_FMT_JPEG;
        break;
      case DEV_H264_ENC:
        _slots[i]->path = ESP32P4_V4L2_DEV_H264_ENC;
        _slots[i]->out_fourcc = V4L2_PIX_FMT_YUV420;
        _slots[i]->cap_fourcc = V4L2_PIX_FMT_H264;
        break;
      case DEV_JPEG_DEC:
        _slots[i]->path = ESP32P4_V4L2_DEV_JPEG_DEC;
        _slots[i]->out_fourcc = V4L2_PIX_FMT_JPEG;
#ifdef V4L2_PIX_FMT_BGR565
        _slots[i]->cap_fourcc = V4L2_PIX_FMT_BGR565;
#else
        _slots[i]->cap_fourcc = V4L2_PIX_FMT_RGB565;
#endif
        break;
      case DEV_ISP:
        _slots[i]->path = ESP32P4_V4L2_DEV_ISP;
        _slots[i]->cap_fourcc = V4L2_META_FMT_ESP_ISP_STATS;
        break;
      default:
        break;
    }
  }

  bool ok = false;
  if (_jpeg) {
    ok |= registerDev(DEV_JPEG_ENC);
    ok |= registerDev(DEV_JPEG_DEC);
    _fd_jpeg_enc = ::open(ESP32P4_V4L2_DEV_JPEG_ENC, O_RDWR);
    _fd_jpeg_dec = ::open(ESP32P4_V4L2_DEV_JPEG_DEC, O_RDWR);
  }
  if (_h264) {
    ok |= registerDev(DEV_H264_ENC);
    _fd_h264_enc = ::open(ESP32P4_V4L2_DEV_H264_ENC, O_RDWR);
  }
  if (_isp && _isp->ready()) {
    ok |= registerDev(DEV_ISP);
    _fd_isp = ::open(ESP32P4_V4L2_DEV_ISP, O_RDWR);
  }

  if (ok) {
    Serial.println("V4L2 M2M: video10=JPEG enc  video11=H264  video12=JPEG dec  video20=ISP stats");
  }
  return ok;
}

void ESP32P4_V4l2M2m::end() {
  if (_fd_jpeg_enc >= 0) {
    ::close(_fd_jpeg_enc);
    _fd_jpeg_enc = -1;
  }
  if (_fd_h264_enc >= 0) {
    ::close(_fd_h264_enc);
    _fd_h264_enc = -1;
  }
  if (_fd_jpeg_dec >= 0) {
    ::close(_fd_jpeg_dec);
    _fd_jpeg_dec = -1;
  }
  if (_fd_isp >= 0) {
    ::close(_fd_isp);
    _fd_isp = -1;
  }
  for (int i = 0; i < DEV_N; i++) {
    if (_slots[i]) {
      unregisterDev((DevKind)i);
      if (_slots[i]->scratch) free(_slots[i]->scratch);
      delete _slots[i];
      _slots[i] = nullptr;
    }
  }
  _cam = nullptr;
  _jpeg = nullptr;
  _h264 = nullptr;
  _isp = nullptr;
}

bool ESP32P4_V4l2M2m::registerDev(DevKind k) {
  DevSlot *d = _slots[k];
  if (!d || d->registered) return d && d->registered;
  if (!d->path || d->path[0] != '/') return false;
  esp_vfs_t vfs = {};
  vfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;
  vfs.open_p = ESP32P4_V4l2M2mVfs::open_p;
  vfs.close_p = ESP32P4_V4l2M2mVfs::close_p;
  vfs.read_p = ESP32P4_V4l2M2mVfs::read_p;
  vfs.write_p = ESP32P4_V4l2M2mVfs::write_p;
  vfs.ioctl_p = ESP32P4_V4l2M2mVfs::ioctl_p;
  vfs.fstat_p = ESP32P4_V4l2M2mVfs::fstat_p;
  if (esp_vfs_register(d->path, &vfs, d) != ESP_OK) return false;
  d->registered = true;
  return true;
}

void ESP32P4_V4l2M2m::unregisterDev(DevKind k) {
  DevSlot *d = _slots[k];
  if (!d || !d->registered) return;
  (void)esp_vfs_unregister(d->path);
  d->registered = false;
}

int ESP32P4_V4l2M2m::devOpen(DevKind k) {
  DevSlot *d = _slots[k];
  if (!d) return fail(ENODEV);
  if (d->open_n > 8) return fail(EMFILE);
  d->open_n++;
  return 0;
}

int ESP32P4_V4l2M2m::devClose(DevKind k) {
  DevSlot *d = _slots[k];
  if (!d) return fail(ENODEV);
  if (d->open_n) d->open_n--;
  d->out_streaming = d->cap_streaming = false;
  d->out_queued = d->cap_queued = d->cap_ready = false;
  return 0;
}

bool ESP32P4_V4l2M2m::fillIspStats(void *dst, size_t cap, size_t *out_len) {
  if (!dst || !out_len) return false;
#if __has_include("esp_video_isp_ioctl.h")
  if (cap < sizeof(esp_video_isp_stats_t)) return false;
  if (_isp && _isp->exportV4l2Stats((esp_video_isp_stats_t *)dst)) {
    *out_len = sizeof(esp_video_isp_stats_t);
    return true;
  }
  memset(dst, 0, sizeof(esp_video_isp_stats_t));
  auto *st = (esp_video_isp_stats_t *)dst;
  st->flags = ESP_VIDEO_ISP_STATS_FLAG_AE | ESP_VIDEO_ISP_STATS_FLAG_AWB;
  *out_len = sizeof(esp_video_isp_stats_t);
  return true;
#else
  return false;
#endif
}

bool ESP32P4_V4l2M2m::tryEncode(DevSlot *d) {
  if (!d || !d->out_queued || !d->cap_queued || !d->out_ptr || !d->cap_ptr) return false;
  if (!d->out_streaming || !d->cap_streaming) return false;

  size_t n = 0;
  switch (d->kind) {
    case DEV_JPEG_ENC: {
      if (!_jpeg) return false;
      esp32p4_cam_pixformat_t pix = ESP32P4_PIXFORMAT_RGB565;
      if (!pix_from_fourcc(d->out_fourcc, &pix)) return false;
      n = _jpeg->encode(d->out_ptr, d->out_w, d->out_h, pix, d->cap_ptr, d->cap_cap);
      break;
    }
    case DEV_H264_ENC: {
      if (!_h264 || !_h264->ready()) return false;
      esp32p4_cam_pixformat_t pix = ESP32P4_PIXFORMAT_RGB565;
      if (!pix_from_fourcc(d->out_fourcc, &pix)) return false;
      if (pix == ESP32P4_PIXFORMAT_RGB565) {
        n = _h264->encode(d->out_ptr, d->out_w, d->out_h, d->cap_ptr, d->cap_cap);
      } else {
        camera_fb_t fb = {};
        fb.buf = d->out_ptr;
        fb.width = d->out_w;
        fb.height = d->out_h;
        fb.format = pix;
        fb.len = d->out_len ? d->out_len : frame_bytes(d->out_fourcc, d->out_w, d->out_h);
        n = _h264->encode(&fb, d->cap_ptr, d->cap_cap);
      }
      break;
    }
    case DEV_JPEG_DEC: {
      if (!_jpeg) return false;
      uint32_t dw = 0, dh = 0;
      n = _jpeg->decode(d->out_ptr, d->out_len, d->cap_ptr, d->cap_cap, &dw, &dh);
      if (n) {
        d->cap_w = (uint16_t)dw;
        d->cap_h = (uint16_t)dh;
      }
      break;
    }
    default:
      return false;
  }
  if (!n) return false;
  d->cap_len = n;
  d->cap_ready = true;
  d->out_queued = false;
  return true;
}

ssize_t ESP32P4_V4l2M2m::devRead(DevKind k, void *dst, size_t n) {
  DevSlot *d = _slots[k];
  if (!d || !dst || !n) return fail(EINVAL);

  if (k == DEV_ISP) {
    size_t len = 0;
    if (!fillIspStats(dst, n, &len)) return fail(EAGAIN);
    return (ssize_t)len;
  }

  if (!d->cap_ready) {
    if (!tryEncode(d)) return fail(EAGAIN);
  }
  size_t copy = d->cap_len < n ? d->cap_len : n;
  if (d->cap_ptr != dst) memcpy(dst, d->cap_ptr, copy);
  d->cap_ready = false;
  d->cap_queued = false;
  return (ssize_t)copy;
}

ssize_t ESP32P4_V4l2M2m::devWrite(DevKind k, const void *src, size_t n) {
  DevSlot *d = _slots[k];
  if (!d || !src || !n) return fail(EINVAL);
  if (k == DEV_ISP) return fail(EINVAL);

  if (!d->scratch || d->scratch_cap < n) {
    uint8_t *p = (uint8_t *)realloc(d->scratch, n);
    if (!p) return fail(ENOMEM);
    d->scratch = p;
    d->scratch_cap = n;
  }
  memcpy(d->scratch, src, n);
  d->out_ptr = d->scratch;
  d->out_len = n;
  d->out_queued = true;

  if (!d->cap_ptr) {
    size_t cap_need = (k == DEV_JPEG_DEC) ? frame_bytes(d->cap_fourcc, d->out_w, d->out_h)
                                          : (d->cap_cap ? d->cap_cap : frame_bytes(d->cap_fourcc, d->cap_w, d->cap_h));
    if (cap_need < 64 * 1024) cap_need = 64 * 1024;
    if (!d->scratch_cap || d->scratch_cap < cap_need + n) {
      uint8_t *p = (uint8_t *)realloc(d->scratch, cap_need + n);
      if (!p) return fail(ENOMEM);
      d->scratch = p;
      d->scratch_cap = cap_need + n;
    }
    d->cap_ptr = d->scratch + n;
    d->cap_cap = cap_need;
  }
  d->cap_queued = true;
  d->out_streaming = d->cap_streaming = true;
  if (!tryEncode(d)) return (ssize_t)n;
  return (ssize_t)n;
}

int ESP32P4_V4l2M2m::devFstat(DevKind k, void *stv) {
  auto *st = (struct stat *)stv;
  if (!st) return fail(EINVAL);
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFCHR | 0666;
  (void)k;
  return 0;
}

int ESP32P4_V4l2M2m::devIoctl(DevKind k, unsigned long request, void *arg) {
  DevSlot *d = _slots[k];
  if (!d) return fail(ENODEV);

  if (request == VIDIOC_QUERYCAP) {
    auto *cap = (struct v4l2_capability *)arg;
    if (!cap) return fail(EINVAL);
    memset(cap, 0, sizeof(*cap));
    strncpy((char *)cap->driver, "csi_vision_m2m", sizeof(cap->driver) - 1);
    strncpy((char *)cap->card, dev_card(k), sizeof(cap->card) - 1);
    strncpy((char *)cap->bus_info, "platform", sizeof(cap->bus_info) - 1);
    cap->version = (3u << 16) | (18u << 8);
    if (k == DEV_ISP) {
      cap->capabilities = V4L2_CAP_META_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE |
                          V4L2_CAP_DEVICE_CAPS;
      cap->device_caps = V4L2_CAP_META_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
    } else {
      cap->capabilities = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE |
                          V4L2_CAP_DEVICE_CAPS;
      cap->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
    }
    return 0;
  }

#ifdef VIDIOC_SET_OWNER
  if (request == VIDIOC_SET_OWNER) {
    d->owner_pid = arg ? *(int *)arg : 0;
    return 0;
  }
#endif

  if (request == VIDIOC_G_FMT || request == VIDIOC_TRY_FMT) {
    auto *fmt = (struct v4l2_format *)arg;
    if (!fmt) return fail(EINVAL);
    if (k == DEV_ISP) {
      if (fmt->type && fmt->type != V4L2_BUF_TYPE_META_CAPTURE) return fail(EINVAL);
      fmt->type = V4L2_BUF_TYPE_META_CAPTURE;
      fmt->fmt.meta.dataformat = V4L2_META_FMT_ESP_ISP_STATS;
#if __has_include("esp_video_isp_ioctl.h")
      fmt->fmt.meta.buffersize = sizeof(esp_video_isp_stats_t);
#else
      fmt->fmt.meta.buffersize = 512;
#endif
      return 0;
    }
    bool capture = (fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if (fmt->type && fmt->type != V4L2_BUF_TYPE_VIDEO_OUTPUT &&
        fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
      return fail(EINVAL);
    if (!fmt->type) fmt->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    auto &pix = fmt->fmt.pix;
    if (capture || fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
      pix.width = d->cap_w;
      pix.height = d->cap_h;
      pix.pixelformat = d->cap_fourcc;
      pix.sizeimage = (d->cap_fourcc == V4L2_PIX_FMT_JPEG || d->cap_fourcc == V4L2_PIX_FMT_H264)
                          ? (uint32_t)(d->cap_w * d->cap_h)
                          : (uint32_t)frame_bytes(d->cap_fourcc, d->cap_w, d->cap_h);
    } else {
      pix.width = d->out_w;
      pix.height = d->out_h;
      pix.pixelformat = d->out_fourcc;
      pix.sizeimage = (uint32_t)frame_bytes(d->out_fourcc, d->out_w, d->out_h);
    }
    pix.field = V4L2_FIELD_NONE;
    pix.bytesperline = pix.width * ((pix.pixelformat == V4L2_PIX_FMT_RGB565 ||
                                     pix.pixelformat == V4L2_PIX_FMT_BGR565)
                                        ? 2
                                        : 0);
    pix.colorspace = V4L2_COLORSPACE_SRGB;
    return 0;
  }

  if (request == VIDIOC_S_FMT) {
    auto *fmt = (struct v4l2_format *)arg;
    if (!fmt) return fail(EINVAL);
    if (k == DEV_ISP) return devIoctl(k, VIDIOC_G_FMT, arg);
    bool capture = (fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE);
    auto &pix = fmt->fmt.pix;
    if (capture) {
      d->cap_w = pix.width ? pix.width : d->cap_w;
      d->cap_h = pix.height ? pix.height : d->cap_h;
      if (pix.pixelformat) d->cap_fourcc = pix.pixelformat;
    } else {
      d->out_w = pix.width ? pix.width : d->out_w;
      d->out_h = pix.height ? pix.height : d->out_h;
      if (pix.pixelformat) d->out_fourcc = pix.pixelformat;
    }
    return devIoctl(k, VIDIOC_G_FMT, arg);
  }

  if (request == VIDIOC_ENUM_FMT && is_m2m(k)) {
    auto *f = (struct v4l2_fmtdesc *)arg;
    if (!f) return fail(EINVAL);
    static const uint32_t kJpegIn[] = {V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_UYVY,
                                       V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_GREY, V4L2_PIX_FMT_YUV420};
    static const uint32_t kH264In[] = {V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_UYVY,
                                       V4L2_PIX_FMT_YUYV};
    const uint32_t *arr = nullptr;
    int arr_n = 0;
    if (k == DEV_JPEG_ENC && f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
      arr = kJpegIn;
      arr_n = (int)(sizeof(kJpegIn) / sizeof(kJpegIn[0]));
    } else if (k == DEV_JPEG_ENC && f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
      if (f->index != 0) return fail(EINVAL);
      f->flags = V4L2_FMT_FLAG_COMPRESSED;
      f->pixelformat = V4L2_PIX_FMT_JPEG;
      strncpy((char *)f->description, "JPEG", sizeof(f->description) - 1);
      return 0;
    } else if (k == DEV_H264_ENC && f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
      arr = kH264In;
      arr_n = (int)(sizeof(kH264In) / sizeof(kH264In[0]));
    } else if (k == DEV_H264_ENC && f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
      if (f->index != 0) return fail(EINVAL);
      f->flags = V4L2_FMT_FLAG_COMPRESSED;
      f->pixelformat = V4L2_PIX_FMT_H264;
      strncpy((char *)f->description, "H264", sizeof(f->description) - 1);
      return 0;
    } else if (k == DEV_JPEG_DEC && f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
      if (f->index != 0) return fail(EINVAL);
      f->flags = V4L2_FMT_FLAG_COMPRESSED;
      f->pixelformat = V4L2_PIX_FMT_JPEG;
      strncpy((char *)f->description, "JPEG", sizeof(f->description) - 1);
      return 0;
    } else if (k == DEV_JPEG_DEC && f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
      if (f->index > 1) return fail(EINVAL);
      f->pixelformat = (f->index == 0) ? V4L2_PIX_FMT_RGB565
#ifdef V4L2_PIX_FMT_BGR565
                                         : V4L2_PIX_FMT_BGR565;
#else
                                         : V4L2_PIX_FMT_RGB565;
#endif
      strncpy((char *)f->description, (f->index == 0) ? "RGB565" : "BGR565", sizeof(f->description) - 1);
      return 0;
    } else {
      return fail(EINVAL);
    }
    if ((int)f->index >= arr_n) return fail(EINVAL);
    f->pixelformat = arr[f->index];
    return 0;
  }

  if (request == VIDIOC_REQBUFS) {
    auto *r = (struct v4l2_requestbuffers *)arg;
    if (!r) return fail(EINVAL);
    if (r->count == 0) {
      d->out_queued = d->cap_queued = d->cap_ready = false;
      return 0;
    }
    r->count = r->count < 2 ? 2 : (r->count > 4 ? 4 : r->count);
    r->capabilities = V4L2_BUF_CAP_SUPPORTS_USERPTR | V4L2_BUF_CAP_SUPPORTS_MMAP;
    return 0;
  }

  if (request == VIDIOC_QUERYBUF) {
    auto *b = (struct v4l2_buffer *)arg;
    if (!b || b->index > 3) return fail(EINVAL);
    uint32_t idx = b->index;
    bool capture = (b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) || (b->type == V4L2_BUF_TYPE_META_CAPTURE);
    memset(b, 0, sizeof(*b));
    b->index = idx;
    b->type = k == DEV_ISP ? V4L2_BUF_TYPE_META_CAPTURE
                           : (capture ? V4L2_BUF_TYPE_VIDEO_CAPTURE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
    b->memory = V4L2_MEMORY_USERPTR;
    if (k == DEV_ISP) {
#if __has_include("esp_video_isp_ioctl.h")
      b->length = sizeof(esp_video_isp_stats_t);
#else
      b->length = 512;
#endif
    } else if (capture) {
      b->length = (d->cap_fourcc == V4L2_PIX_FMT_JPEG || d->cap_fourcc == V4L2_PIX_FMT_H264)
                      ? (uint32_t)(d->cap_w * d->cap_h)
                      : (uint32_t)frame_bytes(d->cap_fourcc, d->cap_w, d->cap_h);
    } else {
      b->length = (uint32_t)frame_bytes(d->out_fourcc, d->out_w, d->out_h);
    }
    b->m.offset = b->index * 4096u;
    b->flags = V4L2_BUF_FLAG_MAPPED;
    return 0;
  }

  if (request == VIDIOC_QBUF) {
    auto *b = (struct v4l2_buffer *)arg;
    if (!b) return fail(EINVAL);
    bool capture = (b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) || (b->type == V4L2_BUF_TYPE_META_CAPTURE);
    if (capture) {
      d->cap_ptr = (uint8_t *)(uintptr_t)b->m.userptr;
      d->cap_cap = b->length;
      d->cap_queued = true;
      if (k == DEV_ISP && d->cap_ptr) {
        size_t len = 0;
        if (fillIspStats(d->cap_ptr, d->cap_cap, &len)) {
          b->bytesused = (uint32_t)len;
          b->flags = V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
          b->sequence = d->seq++;
          d->cap_ready = true;
        }
      } else {
        (void)tryEncode(d);
      }
    } else {
      d->out_ptr = (uint8_t *)(uintptr_t)b->m.userptr;
      d->out_len = b->bytesused ? b->bytesused : b->length;
      d->out_queued = true;
      (void)tryEncode(d);
    }
    b->flags = V4L2_BUF_FLAG_QUEUED;
    return 0;
  }

  if (request == VIDIOC_DQBUF) {
    auto *b = (struct v4l2_buffer *)arg;
    if (!b) return fail(EINVAL);
    bool capture = (b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) || (b->type == V4L2_BUF_TYPE_META_CAPTURE);
    if (capture) {
      if (!d->cap_ready) {
        if (!tryEncode(d)) return fail(EAGAIN);
      }
      memset(b, 0, sizeof(*b));
      b->type = k == DEV_ISP ? V4L2_BUF_TYPE_META_CAPTURE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
      b->memory = V4L2_MEMORY_USERPTR;
      b->m.userptr = (unsigned long)d->cap_ptr;
      b->bytesused = (uint32_t)d->cap_len;
      b->length = (uint32_t)d->cap_cap;
      b->flags = V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
      b->sequence = d->seq++;
      d->cap_ready = false;
      d->cap_queued = false;
      return 0;
    }
    if (!d->out_queued) return fail(EAGAIN);
    memset(b, 0, sizeof(*b));
    b->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    b->memory = V4L2_MEMORY_USERPTR;
    b->m.userptr = (unsigned long)d->out_ptr;
    b->bytesused = (uint32_t)d->out_len;
    b->length = (uint32_t)d->out_len;
    b->flags = V4L2_BUF_FLAG_DONE;
    b->sequence = d->seq++;
    d->out_queued = false;
    return 0;
  }

  if (request == VIDIOC_STREAMON) {
    auto *t = (enum v4l2_buf_type *)arg;
    if (!t) return fail(EINVAL);
    if (*t == V4L2_BUF_TYPE_VIDEO_OUTPUT) d->out_streaming = true;
    else if (*t == V4L2_BUF_TYPE_VIDEO_CAPTURE || *t == V4L2_BUF_TYPE_META_CAPTURE)
      d->cap_streaming = true;
    (void)tryEncode(d);
    return 0;
  }

  if (request == VIDIOC_STREAMOFF) {
    auto *t = (enum v4l2_buf_type *)arg;
    if (!t) return fail(EINVAL);
    if (*t == V4L2_BUF_TYPE_VIDEO_OUTPUT) d->out_streaming = false;
    else if (*t == V4L2_BUF_TYPE_VIDEO_CAPTURE || *t == V4L2_BUF_TYPE_META_CAPTURE)
      d->cap_streaming = false;
    d->out_queued = d->cap_queued = d->cap_ready = false;
    return 0;
  }

  if (request == VIDIOC_G_EXT_CTRLS || request == VIDIOC_S_EXT_CTRLS ||
      request == VIDIOC_TRY_EXT_CTRLS) {
#if __has_include("esp_video_isp_ioctl.h")
    auto *cs = (struct v4l2_ext_controls *)arg;
    if (!cs || !cs->controls) return fail(EINVAL);
    if (k != DEV_ISP || !_isp) return fail(EINVAL);
    for (uint32_t i = 0; i < cs->count; i++) {
      cs->error_idx = i;
      auto &c = cs->controls[i];
      void *ptr = c.ptr ? c.ptr : (void *)c.p_u8;
      if (!ptr || !c.size) return fail(EINVAL);
      if (request == VIDIOC_G_EXT_CTRLS) {
        if (!_isp->exportV4l2Cid(c.id, ptr, c.size)) return fail(EINVAL);
      } else if (request != VIDIOC_TRY_EXT_CTRLS) {
        if (!_isp->importV4l2Cid(c.id, ptr, c.size)) return fail(EINVAL);
      }
    }
    cs->error_idx = cs->count;
    return 0;
#else
    return fail(ENOTTY);
#endif
  }

  if (request == VIDIOC_G_CTRL || request == VIDIOC_S_CTRL) {
    auto *c = (struct v4l2_control *)arg;
    if (!c) return fail(EINVAL);
    if (k == DEV_JPEG_ENC && _jpeg) {
      if (c->id == V4L2_CID_JPEG_COMPRESSION_QUALITY) {
        if (request == VIDIOC_S_CTRL) _jpeg->setQuality((uint8_t)c->value);
        else c->value = _jpeg->quality();
        return 0;
      }
      if (c->id == V4L2_CID_JPEG_CHROMA_SUBSAMPLING) {
        if (request == VIDIOC_S_CTRL) {
          esp32p4_jpeg_chroma_t ch = ESP32P4_JPEG_CHROMA_AUTO;
          switch (c->value) {
            case V4L2_JPEG_CHROMA_SUBSAMPLING_444:
              ch = ESP32P4_JPEG_CHROMA_YUV444;
              break;
            case V4L2_JPEG_CHROMA_SUBSAMPLING_422:
              ch = ESP32P4_JPEG_CHROMA_YUV422;
              break;
            case V4L2_JPEG_CHROMA_SUBSAMPLING_420:
              ch = ESP32P4_JPEG_CHROMA_YUV420;
              break;
            default:
              break;
          }
          _jpeg->setChroma(ch);
        } else {
          switch (_jpeg->chroma()) {
            case ESP32P4_JPEG_CHROMA_YUV444:
              c->value = V4L2_JPEG_CHROMA_SUBSAMPLING_444;
              break;
            case ESP32P4_JPEG_CHROMA_YUV422:
              c->value = V4L2_JPEG_CHROMA_SUBSAMPLING_422;
              break;
            case ESP32P4_JPEG_CHROMA_YUV420:
              c->value = V4L2_JPEG_CHROMA_SUBSAMPLING_420;
              break;
            default:
              c->value = V4L2_JPEG_CHROMA_SUBSAMPLING_422;
              break;
          }
        }
        return 0;
      }
    }
    if (k == DEV_H264_ENC && _h264 && _h264->ready()) {
      switch (c->id) {
        case V4L2_CID_MPEG_VIDEO_BITRATE:
          if (request == VIDIOC_S_CTRL) return _h264->setBitrate((uint32_t)c->value) ? 0 : fail(EINVAL);
          c->value = (int32_t)_h264->bitrate();
          return 0;
        case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
          if (request == VIDIOC_S_CTRL) return _h264->setGop((uint8_t)c->value) ? 0 : fail(EINVAL);
          c->value = _h264->gop();
          return 0;
        case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
          if (request == VIDIOC_S_CTRL) return _h264->setGop((uint8_t)c->value) ? 0 : fail(EINVAL);
          c->value = _h264->gop();
          return 0;
        case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
          if (request == VIDIOC_G_CTRL) {
            c->value = 20;
            return 0;
          }
          return 0;
        case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
          if (request == VIDIOC_G_CTRL) {
            c->value = 45;
            return 0;
          }
          return 0;
        case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
          if (request == VIDIOC_S_CTRL) return _h264->forceIdr() ? 0 : fail(EINVAL);
          c->value = 0;
          return 0;
        default:
          break;
      }
    }
    return fail(EINVAL);
  }

  if (request == VIDIOC_QUERYCTRL && k == DEV_JPEG_ENC) {
    auto *qc = (struct v4l2_queryctrl *)arg;
    if (!qc) return fail(EINVAL);
    if (qc->id == V4L2_CID_JPEG_COMPRESSION_QUALITY || qc->id == V4L2_CID_JPEG_CHROMA_SUBSAMPLING ||
        (qc->id & V4L2_CTRL_FLAG_NEXT_CTRL)) {
      memset(qc, 0, sizeof(*qc));
      if (qc->id == V4L2_CID_JPEG_CHROMA_SUBSAMPLING ||
          (qc->id & V4L2_CTRL_FLAG_NEXT_CTRL && qc->id < V4L2_CID_JPEG_CHROMA_SUBSAMPLING)) {
        qc->id = V4L2_CID_JPEG_CHROMA_SUBSAMPLING;
        qc->type = V4L2_CTRL_TYPE_MENU;
        strncpy((char *)qc->name, "Chroma Subsampling", sizeof(qc->name) - 1);
        qc->minimum = V4L2_JPEG_CHROMA_SUBSAMPLING_444;
        qc->maximum = V4L2_JPEG_CHROMA_SUBSAMPLING_420;
        return 0;
      }
      qc->id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
      qc->type = V4L2_CTRL_TYPE_INTEGER;
      strncpy((char *)qc->name, "Compression Quality", sizeof(qc->name) - 1);
      qc->minimum = 1;
      qc->maximum = 100;
      qc->step = 1;
      qc->default_value = 45;
      return 0;
    }
  }

  return fail(ENOTTY);
}

#else  // !ESP32P4_V4L2M2M_HAS_V4L2

bool ESP32P4_V4l2M2m::begin(const Config &) { return false; }
void ESP32P4_V4l2M2m::end() {}
bool ESP32P4_V4l2M2m::registerDev(DevKind) { return false; }
void ESP32P4_V4l2M2m::unregisterDev(DevKind) {}
int ESP32P4_V4l2M2m::devOpen(DevKind) { return -1; }
int ESP32P4_V4l2M2m::devClose(DevKind) { return -1; }
ssize_t ESP32P4_V4l2M2m::devRead(DevKind, void *, size_t) { return -1; }
ssize_t ESP32P4_V4l2M2m::devWrite(DevKind, const void *, size_t) { return -1; }
int ESP32P4_V4l2M2m::devFstat(DevKind, void *) { return -1; }
int ESP32P4_V4l2M2m::devIoctl(DevKind, unsigned long, void *) { return -1; }
bool ESP32P4_V4l2M2m::tryEncode(DevSlot *) { return false; }
bool ESP32P4_V4l2M2m::fillIspStats(void *, size_t, size_t *) { return false; }

#endif
