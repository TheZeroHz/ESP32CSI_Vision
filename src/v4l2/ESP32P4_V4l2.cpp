#include "v4l2/ESP32P4_V4l2.h"

#include <Arduino.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "cam/ESP32P4_Isp.h"

#if ESP32P4_HAS_V4L2
#include "esp_vfs.h"
#if __has_include("esp_video_ioctl.h")
#include "esp_video_ioctl.h"
#endif
#if __has_include("esp_video_isp_ioctl.h")
#include "esp_video_isp_ioctl.h"
#endif
#if __has_include("esp_cam_sensor_types.h")
#include "esp_cam_sensor_types.h"
#endif
#if __has_include("esp_cam_motor_types.h")
#include "esp_cam_motor_types.h"
#endif

#ifndef V4L2_CID_CAMERA_AE_LEVEL
#define V4L2_CID_CAMERA_AE_LEVEL (V4L2_CID_CAMERA_CLASS_BASE + 40)
#endif
#ifndef V4L2_CID_TEST_PATTERN
#define V4L2_CID_TEST_PATTERN (V4L2_CID_IMAGE_PROC_CLASS_BASE + 3)
#endif
#ifndef V4L2_CAP_TIMEPERFRAME
#define V4L2_CAP_TIMEPERFRAME 0x1000
#endif
#ifndef V4L2_BUF_CAP_SUPPORTS_USERPTR
#define V4L2_BUF_CAP_SUPPORTS_USERPTR (1 << 1)
#endif
#ifndef V4L2_BUF_CAP_SUPPORTS_MMAP
#define V4L2_BUF_CAP_SUPPORTS_MMAP (1 << 0)
#endif

struct ESP32P4_V4l2Vfs {
  static int open_p(void *ctx, const char *, int, int) {
    auto *self = (ESP32P4_V4l2 *)ctx;
    return self ? self->vfsOpen() : -1;
  }
  static int close_p(void *ctx, int fd) {
    auto *self = (ESP32P4_V4l2 *)ctx;
    return self ? self->vfsClose(fd) : -1;
  }
  static ssize_t read_p(void *ctx, int fd, void *dst, size_t n) {
    auto *self = (ESP32P4_V4l2 *)ctx;
    return self ? self->vfsRead(fd, dst, n) : -1;
  }
  static int ioctl_p(void *ctx, int fd, int cmd, va_list args) {
    auto *self = (ESP32P4_V4l2 *)ctx;
    void *arg = va_arg(args, void *);
    return self ? self->vfsIoctl(fd, cmd, arg) : -1;
  }
  static int fstat_p(void *ctx, int fd, struct stat *st) {
    auto *self = (ESP32P4_V4l2 *)ctx;
    return self ? self->vfsFstat(fd, st) : -1;
  }
};

struct CtrlDesc {
  uint32_t id;
  const char *name;
  const char *ctl;
  int32_t minv, maxv, step, defv;
  uint32_t type;
};

static const CtrlDesc kCtrls[] = {
    {V4L2_CID_BRIGHTNESS, "Brightness", "brightness", -128, 127, 1, 0, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_CONTRAST, "Contrast", "contrast", 0, 255, 1, 128, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_SATURATION, "Saturation", "saturation", 0, 255, 1, 128, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_HUE, "Hue", "hue", 0, 359, 1, 0, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_AUTO_WHITE_BALANCE, "White Balance, Automatic", "white_balance_automatic", 0, 1, 1, 1,
     V4L2_CTRL_TYPE_BOOLEAN},
    {V4L2_CID_RED_BALANCE, "Red Balance", "red_balance", 256, 4096, 1, 1024, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_BLUE_BALANCE, "Blue Balance", "blue_balance", 256, 4096, 1, 1024,
     V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_EXPOSURE, "Exposure", "exposure", 4, 65535, 1, 200, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_AUTOGAIN, "Automatic Gain", "autogain", 0, 1, 1, 1, V4L2_CTRL_TYPE_BOOLEAN},
    {V4L2_CID_GAIN, "Gain", "gain", 1, 480, 1, 16, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_HFLIP, "Horizontal Flip", "horizontal_flip", 0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN},
    {V4L2_CID_VFLIP, "Vertical Flip", "vertical_flip", 0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN},
    {V4L2_CID_POWER_LINE_FREQUENCY, "Power Line Frequency", "power_line_frequency", 0, 2, 1, 0,
     V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_SHARPNESS, "Sharpness", "sharpness", 0, 255, 1, 128, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_EXPOSURE_ABSOLUTE, "Exposure Time, Absolute", "exposure_time_absolute", 1, 100000, 1,
     100, V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_FOCUS_ABSOLUTE, "Focus, Absolute", "focus_absolute", 0, 1023, 1, 0,
     V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_JPEG_COMPRESSION_QUALITY, "Compression Quality", "compression_quality", 1, 100, 1, 45,
     V4L2_CTRL_TYPE_INTEGER},
    {V4L2_CID_TEST_PATTERN, "Test Pattern", "test_pattern", 0, 1, 1, 0, V4L2_CTRL_TYPE_BOOLEAN},
    {V4L2_CID_CAMERA_AE_LEVEL, "AE Target Level", "camera_ae_level", 20, 180, 1, 80,
     V4L2_CTRL_TYPE_INTEGER},
};

static constexpr int kCtrlN = (int)(sizeof(kCtrls) / sizeof(kCtrls[0]));

static const CtrlDesc *findCtrl(uint32_t id) {
  for (int i = 0; i < kCtrlN; i++) {
    if (kCtrls[i].id == id) return &kCtrls[i];
  }
  return nullptr;
}

static const CtrlDesc *findCtrlName(const char *n) {
  if (!n) return nullptr;
  for (int i = 0; i < kCtrlN; i++) {
    if (!strcasecmp(kCtrls[i].ctl, n)) return &kCtrls[i];
  }
  return nullptr;
}

struct FmtDesc {
  esp32p4_cam_pixformat_t pix;
  uint32_t fourcc;
  const char *name;
};

static const FmtDesc kFmts[] = {
    {ESP32P4_PIXFORMAT_RGB565, V4L2_PIX_FMT_RGB565, "RGB565"},
    {ESP32P4_PIXFORMAT_RGB888, V4L2_PIX_FMT_RGB24, "RGB888"},
    {ESP32P4_PIXFORMAT_YUV422, V4L2_PIX_FMT_UYVY, "UYVY"},
    {ESP32P4_PIXFORMAT_YUYV, V4L2_PIX_FMT_YUYV, "YUYV"},
    {ESP32P4_PIXFORMAT_YUV420, V4L2_PIX_FMT_YUV420, "YUV420"},
    {ESP32P4_PIXFORMAT_GRAY8, V4L2_PIX_FMT_GREY, "GRAY8"},
    {ESP32P4_PIXFORMAT_RAW8, V4L2_PIX_FMT_SRGGB8, "RAW8"},
    {ESP32P4_PIXFORMAT_RAW10, V4L2_PIX_FMT_SGRBG10, "RAW10"},
    {ESP32P4_PIXFORMAT_RAW12, V4L2_PIX_FMT_SGRBG12, "RAW12"},
    {ESP32P4_PIXFORMAT_JPEG, V4L2_PIX_FMT_JPEG, "JPEG"},
};
static constexpr int kFmtN = (int)(sizeof(kFmts) / sizeof(kFmts[0]));

static void fourcc_str(uint32_t fcc, char out[5]) {
  out[0] = (char)(fcc & 0xff);
  out[1] = (char)((fcc >> 8) & 0xff);
  out[2] = (char)((fcc >> 16) & 0xff);
  out[3] = (char)((fcc >> 24) & 0xff);
  out[4] = 0;
}

static int fail(int err) {
  errno = err;
  return -1;
}

const char *ESP32P4_V4l2::defaultPath(esp32p4_cam_bus_t bus) {
  switch (bus) {
    case ESP32P4_CAM_BUS_DVP:
      return ESP32P4_V4L2_DEV_DVP;
    case ESP32P4_CAM_BUS_SPI:
      return ESP32P4_V4L2_DEV_SPI;
    case ESP32P4_CAM_BUS_UVC_HOST:
      return ESP32P4_V4L2_DEV_UVC;
    case ESP32P4_CAM_BUS_CSI:
    default:
      return ESP32P4_V4L2_DEV_CSI;
  }
}

const char *ESP32P4_V4l2::defaultPath(ESP32P4_Camera *cam) {
  if (!cam) return ESP32P4_V4L2_DEV_CSI;
  if (cam->bus() == ESP32P4_CAM_BUS_SPI) {
    uint8_t p = cam->spiPort();
    if (p != 0 && p != 1) return ESP32P4_V4L2_DEV_SPI1;
  }
  return defaultPath(cam->bus());
}

uint32_t ESP32P4_V4l2::pixToFourcc(esp32p4_cam_pixformat_t fmt) {
  for (int i = 0; i < kFmtN; i++) {
    if (kFmts[i].pix == fmt) return kFmts[i].fourcc;
  }
  return V4L2_PIX_FMT_RGB565;
}

bool ESP32P4_V4l2::fourccToPix(uint32_t fourcc, esp32p4_cam_pixformat_t *out) {
  if (!out) return false;
  for (int i = 0; i < kFmtN; i++) {
    if (kFmts[i].fourcc == fourcc) {
      *out = kFmts[i].pix;
      return true;
    }
  }
  for (int i = 0; i < kFmtN; i++) {
    if (esp32p4_pixformat_fourcc(kFmts[i].pix) == fourcc) {
      *out = kFmts[i].pix;
      return true;
    }
  }
  return false;
}

bool ESP32P4_V4l2::begin(ESP32P4_Camera *cam, const char *path) {
  end();
  if (!cam || !cam->detected()) {
    Serial.println("V4L2: camera not started - call cam.begin() first");
    return false;
  }
  _cam = cam;
  const char *p = path ? path : defaultPath(cam);
  strncpy(_path, p, sizeof(_path) - 1);
  _dq_ms = 2000;
  _seq = 0;
  _req_n = cam->fbCount();
  if (!registerVfs()) {
    Serial.printf("V4L2: VFS %s failed (ioctl API still works)\n", _path);
  } else {
    _fd = ::open(_path, O_RDWR);
    if (_fd < 0) Serial.printf("V4L2: open %s failed errno=%d\n", _path, errno);
  }
  Serial.printf("V4L2: %s  %s  %ux%u  %s  (camera_fb_t still works)\n", _path, cam->sensorName(),
                cam->width(), cam->height(), cam->formatName());
  return true;
}

void ESP32P4_V4l2::end() {
  for (uint8_t i = 0; i < ESP32P4_CAM_FB_MAX; i++) {
    if (_held[i] && _cam) _cam->release(_held[i]);
    _held[i] = nullptr;
  }
  if (_fd >= 0) {
    ::close(_fd);
    _fd = -1;
  }
  unregisterVfs();
  _cam = nullptr;
  _open_n = 0;
}

bool ESP32P4_V4l2::registerVfs() {
  if (_vfs) return true;
  if (_path[0] != '/') return false;
  esp_vfs_t vfs = {};
  vfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;
  vfs.open_p = ESP32P4_V4l2Vfs::open_p;
  vfs.close_p = ESP32P4_V4l2Vfs::close_p;
  vfs.read_p = ESP32P4_V4l2Vfs::read_p;
  vfs.ioctl_p = ESP32P4_V4l2Vfs::ioctl_p;
  vfs.fstat_p = ESP32P4_V4l2Vfs::fstat_p;
  if (esp_vfs_register(_path, &vfs, this) != ESP_OK) return false;
  _vfs = true;
  return true;
}

void ESP32P4_V4l2::unregisterVfs() {
  if (!_vfs) return;
  (void)esp_vfs_unregister(_path);
  _vfs = false;
}

int ESP32P4_V4l2::vfsOpen() {
  if (_open_n > 8) return fail(EMFILE);
  _open_n++;
  return 0;
}

int ESP32P4_V4l2::vfsClose(int) {
  if (_open_n) _open_n--;
  return 0;
}

int ESP32P4_V4l2::vfsRead(int, void *dst, size_t size) {
  if (!_cam || !dst || !size) return fail(EINVAL);
  camera_fb_t *fb = _cam->capture(_dq_ms);
  if (!fb) return fail(EAGAIN);
  size_t n = fb->len < size ? fb->len : size;
  memcpy(dst, fb->buf, n);
  _cam->release(fb);
  return (int)n;
}

int ESP32P4_V4l2::vfsIoctl(int, int cmd, void *arg) {
  return ioctlUnlocked((unsigned long)cmd, arg);
}

int ESP32P4_V4l2::vfsFstat(int, void *stv) {
  auto *st = (struct stat *)stv;
  if (!st) return fail(EINVAL);
  memset(st, 0, sizeof(*st));
  st->st_mode = S_IFCHR | 0666;
  return 0;
}

void *ESP32P4_V4l2::mmap(size_t length, off_t offset) {
  if (!_cam) {
    errno = ENODEV;
    return (void *)-1;
  }
  uint32_t idx = (uint32_t)(offset / 4096);
  uint8_t *p = _cam->fbBuf((uint8_t)idx);
  if (!p) {
    errno = EINVAL;
    return (void *)-1;
  }
  (void)length;
  return p;
}

int ESP32P4_V4l2::munmap(void *, size_t) { return 0; }

int ESP32P4_V4l2::ioctl(unsigned long request, void *arg) { return ioctlUnlocked(request, arg); }

bool ESP32P4_V4l2::fillFmt(void *fmtv) const {
  auto *fmt = (struct v4l2_format *)fmtv;
  if (!_cam || !fmt) return false;
  if (fmt->type && fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) return false;
  fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  auto &pix = fmt->fmt.pix;
  pix.width = _cam->width();
  pix.height = _cam->height();
  pix.pixelformat = pixToFourcc(_cam->format());
  pix.field = V4L2_FIELD_NONE;
  pix.bytesperline = (uint32_t)esp32p4_pixformat_fb_bytes(_cam->format(), _cam->width(), 1);
  if (_cam->format() == ESP32P4_PIXFORMAT_GRAY8) pix.bytesperline = _cam->width();
  pix.sizeimage =
      (uint32_t)esp32p4_pixformat_fb_bytes(_cam->format(), _cam->width(), _cam->height());
  if (_cam->format() == ESP32P4_PIXFORMAT_GRAY8)
    pix.sizeimage = (uint32_t)_cam->width() * _cam->height();
  pix.colorspace = V4L2_COLORSPACE_SRGB;
  return true;
}

bool ESP32P4_V4l2::applyFmt(void *fmtv) {
  auto *fmt = (struct v4l2_format *)fmtv;
  if (!_cam || !fmt) return false;
  if (fmt->type && fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) return false;
  esp32p4_cam_pixformat_t pix = ESP32P4_PIXFORMAT_RGB565;
  if (!fourccToPix(fmt->fmt.pix.pixelformat, &pix)) return false;
  if (!_cam->supportsFormat(pix)) return false;
  if (!_cam->setFormat(pix)) return false;
  return fillFmt(fmt);
}

int ESP32P4_V4l2::ioctlUnlocked(unsigned long request, void *arg) {
  if (!_cam) return fail(ENODEV);
  if (request == VIDIOC_QUERYCAP) {
    auto *cap = (struct v4l2_capability *)arg;
    if (!cap) return fail(EINVAL);
    memset(cap, 0, sizeof(*cap));
    strncpy((char *)cap->driver, "csi_vision", sizeof(cap->driver) - 1);
    strncpy((char *)cap->card, _cam->sensorName() ? _cam->sensorName() : "ESP32-P4",
            sizeof(cap->card) - 1);
    strncpy((char *)cap->bus_info, _cam->busName() ? _cam->busName() : "csi",
            sizeof(cap->bus_info) - 1);
    cap->version = (3u << 16) | (16u << 8);
    cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE |
                        V4L2_CAP_DEVICE_CAPS;
    cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
    return 0;
  }
  if (request == VIDIOC_ENUM_FMT) {
    auto *f = (struct v4l2_fmtdesc *)arg;
    if (!f) return fail(EINVAL);
    uint32_t want = f->index;
    uint32_t n = 0;
    for (int i = 0; i < kFmtN; i++) {
      if (!_cam->supportsFormat(kFmts[i].pix)) continue;
      if (n == want) {
        f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        f->flags = (kFmts[i].pix == ESP32P4_PIXFORMAT_JPEG) ? V4L2_FMT_FLAG_COMPRESSED : 0;
        strncpy((char *)f->description, kFmts[i].name, sizeof(f->description) - 1);
        f->pixelformat = kFmts[i].fourcc;
        return 0;
      }
      n++;
    }
    return fail(EINVAL);
  }
  if (request == VIDIOC_G_FMT || request == VIDIOC_TRY_FMT) {
    if (!fillFmt(arg)) return fail(EINVAL);
    return 0;
  }
  if (request == VIDIOC_S_FMT) {
    if (!applyFmt(arg)) return fail(EINVAL);
    return 0;
  }
  if (request == VIDIOC_ENUM_FRAMESIZES) {
    auto *e = (struct v4l2_frmsizeenum *)arg;
    if (!e || e->index != 0) return fail(EINVAL);
    e->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    e->discrete.width = _cam->width();
    e->discrete.height = _cam->height();
    return 0;
  }
  if (request == VIDIOC_QUERYCTRL) return queryCtrlIoctl(arg);
  if (request == VIDIOC_G_CTRL) {
    auto *c = (struct v4l2_control *)arg;
    if (!c) return fail(EINVAL);
    int32_t v = 0;
    if (!getCtrl(c->id, &v)) return fail(EINVAL);
    c->value = v;
    return 0;
  }
  if (request == VIDIOC_S_CTRL) {
    auto *c = (struct v4l2_control *)arg;
    if (!c) return fail(EINVAL);
    if (!setCtrl(c->id, c->value)) return fail(EINVAL);
    return 0;
  }
  if (request == VIDIOC_G_EXT_CTRLS || request == VIDIOC_S_EXT_CTRLS ||
      request == VIDIOC_TRY_EXT_CTRLS) {
    auto *cs = (struct v4l2_ext_controls *)arg;
    if (!cs || !cs->controls) return fail(EINVAL);
    for (uint32_t i = 0; i < cs->count; i++) {
      cs->error_idx = i;
      auto &c = cs->controls[i];
#ifdef V4L2_CTRL_CLASS_ESP_CAM_IOCTL
      if (cs->ctrl_class == V4L2_CTRL_CLASS_ESP_CAM_IOCTL ||
          (c.id & 0xffff0000u) == V4L2_CTRL_CLASS_ESP_CAM_IOCTL) {
        uint32_t cmd = c.id;
        if ((cmd & 0xffff0000u) == V4L2_CTRL_CLASS_ESP_CAM_IOCTL) cmd &= 0xffff;
        size_t sz = c.size ? c.size : (size_t)ESP_CAM_SENSOR_IOC_GET_ARG(cmd);
        void *ptr = c.ptr ? c.ptr : (void *)c.p_u8;
        if (request == VIDIOC_TRY_EXT_CTRLS) continue;
        if (!_cam->sensorIoctl(cmd, ptr, sz)) return fail(EINVAL);
        continue;
      }
#endif
#if __has_include("esp_video_isp_ioctl.h")
      if (c.id >= V4L2_CID_USER_ESP_ISP_CCM && c.id <= V4L2_CID_USER_ESP_ISP_GAMMA_EXT) {
        ESP32P4_Isp *isp = _cam->isp();
        if (!isp) return fail(ENODEV);
        void *ptr = c.ptr ? c.ptr : (void *)c.p_u8;
        if (!ptr || !c.size) return fail(EINVAL);
        if (request == VIDIOC_G_EXT_CTRLS) {
          if (!isp->exportV4l2Cid(c.id, ptr, c.size)) return fail(EINVAL);
        } else if (request != VIDIOC_TRY_EXT_CTRLS) {
          if (!isp->importV4l2Cid(c.id, ptr, c.size)) return fail(EINVAL);
        }
        continue;
      }
#endif
      if (request == VIDIOC_G_EXT_CTRLS) {
        int32_t v = 0;
        if (!getCtrl(c.id, &v)) return fail(EINVAL);
        c.value = v;
      } else {
        if (!setCtrl(c.id, c.value)) return fail(EINVAL);
      }
    }
    cs->error_idx = cs->count;
    return 0;
  }
  if (request == VIDIOC_REQBUFS) {
    auto *r = (struct v4l2_requestbuffers *)arg;
    if (!r) return fail(EINVAL);
    if (r->count == 0) {
      for (uint8_t i = 0; i < ESP32P4_CAM_FB_MAX; i++) {
        if (_held[i]) {
          _cam->release(_held[i]);
          _held[i] = nullptr;
        }
      }
      _req_n = 0;
      return 0;
    }
    uint32_t n = r->count;
    if (n > _cam->fbCount()) n = _cam->fbCount();
    if (n < 2) n = _cam->fbCount() >= 2 ? 2 : _cam->fbCount();
    r->count = n;
    r->capabilities = V4L2_BUF_CAP_SUPPORTS_USERPTR | V4L2_BUF_CAP_SUPPORTS_MMAP;
    _req_n = n;
    return 0;
  }
  if (request == VIDIOC_QUERYBUF) {
    auto *b = (struct v4l2_buffer *)arg;
    if (!b || b->index >= _req_n || b->index >= ESP32P4_CAM_FB_MAX) return fail(EINVAL);
    b->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b->memory = b->memory ? b->memory : V4L2_MEMORY_MMAP;
    b->length =
        (uint32_t)esp32p4_pixformat_fb_bytes(_cam->format(), _cam->width(), _cam->height());
    b->m.offset = b->index * 4096u;
    b->flags = V4L2_BUF_FLAG_MAPPED;
    return 0;
  }
  if (request == VIDIOC_QBUF) {
    auto *b = (struct v4l2_buffer *)arg;
    if (!b || b->index >= ESP32P4_CAM_FB_MAX) return fail(EINVAL);
    if (_held[b->index]) {
      _cam->release(_held[b->index]);
      _held[b->index] = nullptr;
    }
    b->flags = V4L2_BUF_FLAG_QUEUED;
    return 0;
  }
  if (request == VIDIOC_DQBUF) {
    auto *b = (struct v4l2_buffer *)arg;
    if (!b) return fail(EINVAL);
    camera_fb_t *fb = _cam->capture(_dq_ms);
    if (!fb) return fail(EAGAIN);
    int idx = -1;
    for (uint8_t i = 0; i < ESP32P4_CAM_FB_MAX; i++) {
      if (_cam->fbBuf(i) == fb->buf) {
        idx = (int)i;
        break;
      }
    }
    if (idx < 0) {
      for (uint8_t i = 0; i < ESP32P4_CAM_FB_MAX; i++) {
        if (!_held[i]) {
          idx = (int)i;
          break;
        }
      }
    }
    if (idx < 0) idx = 0;
    uint32_t mem = b->memory ? b->memory : V4L2_MEMORY_USERPTR;
    if (_held[idx]) _cam->release(_held[idx]);
    _held[idx] = fb;
    memset(b, 0, sizeof(*b));
    b->index = (uint32_t)idx;
    b->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b->bytesused = (uint32_t)fb->len;
    b->length =
        (uint32_t)esp32p4_pixformat_fb_bytes(_cam->format(), _cam->width(), _cam->height());
    b->memory = mem;
    if (mem == V4L2_MEMORY_MMAP) {
      b->m.offset = (uint32_t)idx * 4096u;
    } else {
      b->m.userptr = (unsigned long)fb->buf;
    }
    b->flags = V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC | V4L2_BUF_FLAG_MAPPED;
    b->sequence = _seq++;
    b->timestamp.tv_sec = (time_t)(fb->timestamp_us / 1000000u);
    b->timestamp.tv_usec = (suseconds_t)(fb->timestamp_us % 1000000u);
    b->field = V4L2_FIELD_NONE;
    return 0;
  }
  if (request == VIDIOC_STREAMON) {
    (void)_cam->startCapture();
    return 0;
  }
  if (request == VIDIOC_STREAMOFF) {
    for (uint8_t i = 0; i < ESP32P4_CAM_FB_MAX; i++) {
      if (_held[i]) {
        _cam->release(_held[i]);
        _held[i] = nullptr;
      }
    }
    (void)_cam->stopCapture();
    return 0;
  }
  if (request == VIDIOC_G_PARM) {
    auto *p = (struct v4l2_streamparm *)arg;
    if (!p) return fail(EINVAL);
    p->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    p->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
    p->parm.capture.timeperframe.numerator = 1;
    p->parm.capture.timeperframe.denominator = _cam->modeFps() ? _cam->modeFps() : 25;
    p->parm.capture.readbuffers = _cam->fbCount();
    return 0;
  }
  if (request == VIDIOC_S_PARM) {
    auto *p = (struct v4l2_streamparm *)arg;
    if (!p) return fail(EINVAL);
    p->parm.capture.timeperframe.numerator = 1;
    p->parm.capture.timeperframe.denominator = _cam->modeFps() ? _cam->modeFps() : 25;
    return 0;
  }
#ifdef VIDIOC_S_DQBUF_TIMEOUT
  if (request == VIDIOC_S_DQBUF_TIMEOUT) {
    auto *tv = (struct timeval *)arg;
    if (!tv) return fail(EINVAL);
    _dq_ms = (uint32_t)(tv->tv_sec * 1000 + tv->tv_usec / 1000);
    if (!_dq_ms) _dq_ms = 1;
    return 0;
  }
#endif
#ifdef VIDIOC_G_DQBUF_TIMEOUT
  if (request == VIDIOC_G_DQBUF_TIMEOUT) {
    auto *tv = (struct timeval *)arg;
    if (!tv) return fail(EINVAL);
    tv->tv_sec = (time_t)(_dq_ms / 1000);
    tv->tv_usec = (suseconds_t)((_dq_ms % 1000) * 1000);
    return 0;
  }
#endif
#ifdef VIDIOC_G_SENSOR_FMT
  if (request == VIDIOC_G_SENSOR_FMT) {
    auto *sf = (esp_cam_sensor_format_t *)arg;
    if (!sf) return fail(EINVAL);
    memset(sf, 0, sizeof(*sf));
    sf->name = _cam->sensorName();
    sf->width = _cam->width();
    sf->height = _cam->height();
    sf->fps = _cam->modeFps();
    sf->port = (_cam->bus() == ESP32P4_CAM_BUS_DVP)     ? ESP_CAM_SENSOR_DVP
               : (_cam->bus() == ESP32P4_CAM_BUS_SPI)   ? ESP_CAM_SENSOR_SPI
                                                        : ESP_CAM_SENSOR_MIPI_CSI;
    return 0;
  }
#endif
#ifdef VIDIOC_S_SENSOR_FMT
  if (request == VIDIOC_S_SENSOR_FMT) {
    auto *sf = (esp_cam_sensor_format_t *)arg;
    if (!sf) return fail(EINVAL);
    if (sf->width && sf->height && (sf->width != _cam->width() || sf->height != _cam->height())) {
      return fail(EINVAL);
    }
    return 0;
  }
#endif
#ifdef VIDIOC_SET_OWNER
  if (request == VIDIOC_SET_OWNER) {
    _owner = arg ? *(int *)arg : 0;
    return 0;
  }
#endif
#ifdef VIDIOC_G_MOTOR_FMT
  if (request == VIDIOC_G_MOTOR_FMT) {
    auto *mf = (esp_cam_motor_format_t *)arg;
    if (!mf) return fail(EINVAL);
    if (!_cam->afPresent()) return fail(ENODEV);
    memset(mf, 0, sizeof(*mf));
    mf->name = "DW9714";
    mf->mode = ESP_CAM_MOTOR_DIRECT_MODE;
    mf->init_position = (int)_cam->afPosition();
    return 0;
  }
#endif
#ifdef VIDIOC_S_MOTOR_FMT
  if (request == VIDIOC_S_MOTOR_FMT) {
    if (!_cam->afPresent()) return fail(ENODEV);
    return 0;
  }
#endif
  return fail(ENOTTY);
}

int ESP32P4_V4l2::queryCtrlIoctl(void *qcv) {
  auto *qc = (struct v4l2_queryctrl *)qcv;
  if (!qc) return fail(EINVAL);
  uint32_t id = qc->id;
  bool next = (id & V4L2_CTRL_FLAG_NEXT_CTRL) != 0;
  id &= ~V4L2_CTRL_FLAG_NEXT_CTRL;
  const CtrlDesc *d = nullptr;
  if (next) {
    for (int i = 0; i < kCtrlN; i++) {
      if (kCtrls[i].id > id) {
        d = &kCtrls[i];
        break;
      }
    }
  } else {
    d = findCtrl(id);
  }
  if (!d) return fail(EINVAL);
  memset(qc, 0, sizeof(*qc));
  qc->id = d->id;
  qc->type = d->type;
  strncpy((char *)qc->name, d->name, sizeof(qc->name) - 1);
  qc->minimum = d->minv;
  qc->maximum = d->maxv;
  qc->step = d->step;
  qc->default_value = d->defv;
  qc->flags = V4L2_CTRL_FLAG_SLIDER;
  return 0;
}

bool ESP32P4_V4l2::ctrlMeta(uint32_t id, const char **name, int32_t *minv, int32_t *maxv,
                            int32_t *step, int32_t *defv, uint32_t *type) const {
  const CtrlDesc *d = findCtrl(id);
  if (!d) return false;
  if (name) *name = d->name;
  if (minv) *minv = d->minv;
  if (maxv) *maxv = d->maxv;
  if (step) *step = d->step;
  if (defv) *defv = d->defv;
  if (type) *type = d->type;
  return true;
}

bool ESP32P4_V4l2::queryCtrl(uint32_t id, char *name, int32_t *minv, int32_t *maxv, int32_t *step,
                             int32_t *defv) const {
  const char *n = nullptr;
  if (!ctrlMeta(id, &n, minv, maxv, step, defv, nullptr)) return false;
  if (name && n) strncpy(name, n, 31);
  return true;
}

bool ESP32P4_V4l2::setCtrl(uint32_t id, int32_t value) {
  if (!_cam) return false;
  switch (id) {
    case V4L2_CID_BRIGHTNESS:
      return _cam->setBrightness((int8_t)value);
    case V4L2_CID_CONTRAST:
      return _cam->setContrast((uint8_t)value);
    case V4L2_CID_SATURATION:
      return _cam->setSaturation((uint8_t)value);
    case V4L2_CID_HUE:
      return _cam->setHue((uint16_t)value);
    case V4L2_CID_AUTO_WHITE_BALANCE:
      return _cam->setAwb(value != 0);
    case V4L2_CID_RED_BALANCE:
      return _cam->setRedBalance(value);
    case V4L2_CID_BLUE_BALANCE:
      return _cam->setBlueBalance(value);
    case V4L2_CID_EXPOSURE:
      return _cam->setExposure((uint16_t)value);
    case V4L2_CID_AUTOGAIN: {
      bool ok = _cam->setAGC(value != 0);
      (void)_cam->setIspAe(value != 0);
      return ok || _cam->ispReady();
    }
    case V4L2_CID_GAIN:
      return _cam->setGain((uint16_t)value);
    case V4L2_CID_HFLIP:
      return _cam->setHMirror(value != 0);
    case V4L2_CID_VFLIP:
      return _cam->setVFlip(value != 0);
    case V4L2_CID_POWER_LINE_FREQUENCY: {
      uint8_t hz = 0;
      if (value == V4L2_CID_POWER_LINE_FREQUENCY_50HZ) hz = 50;
      else if (value == V4L2_CID_POWER_LINE_FREQUENCY_60HZ) hz = 60;
      return _cam->setAntiFlicker(hz);
    }
    case V4L2_CID_SHARPNESS:
      return _cam->setSharpness((uint8_t)value);
    case V4L2_CID_EXPOSURE_ABSOLUTE:
      return _cam->setExposureTime(value);
    case V4L2_CID_FOCUS_ABSOLUTE:
      if (!_cam->afPresent()) (void)_cam->afBegin();
      return _cam->setAfPosition((uint16_t)value);
    case V4L2_CID_JPEG_COMPRESSION_QUALITY:
      return _cam->setJpegQuality((uint8_t)value);
    case V4L2_CID_TEST_PATTERN:
      return _cam->setTestPattern(value != 0);
    case V4L2_CID_CAMERA_AE_LEVEL:
      return _cam->setAeTarget((uint8_t)value);
    default:
      return false;
  }
}

bool ESP32P4_V4l2::getCtrl(uint32_t id, int32_t *value) const {
  if (!_cam || !value) return false;
  bool b = false;
  uint16_t u16 = 0;
  switch (id) {
    case V4L2_CID_BRIGHTNESS:
      *value = _cam->brightness();
      return true;
    case V4L2_CID_CONTRAST:
      *value = _cam->contrast();
      return true;
    case V4L2_CID_SATURATION:
      *value = _cam->saturation();
      return true;
    case V4L2_CID_HUE:
      *value = _cam->hue();
      return true;
    case V4L2_CID_AUTO_WHITE_BALANCE:
      *value = (_cam->isp() && _cam->isp()->awbEnabled()) ? 1 : 0;
      return true;
    case V4L2_CID_RED_BALANCE:
      *value = _cam->isp() ? _cam->isp()->redBalance() : 1024;
      return true;
    case V4L2_CID_BLUE_BALANCE:
      *value = _cam->isp() ? _cam->isp()->blueBalance() : 1024;
      return true;
    case V4L2_CID_EXPOSURE:
      if (!_cam->getExposure(&u16)) return false;
      *value = u16;
      return true;
    case V4L2_CID_AUTOGAIN:
      if (_cam->getAGC(&b)) {
        *value = b ? 1 : 0;
        return true;
      }
      *value = (_cam->isp() && _cam->isp()->aeEnabled()) ? 1 : 0;
      return true;
    case V4L2_CID_GAIN:
      if (!_cam->getGain(&u16)) return false;
      *value = u16;
      return true;
    case V4L2_CID_HFLIP:
      if (!_cam->getHMirror(&b)) return false;
      *value = b ? 1 : 0;
      return true;
    case V4L2_CID_VFLIP:
      if (!_cam->getVFlip(&b)) return false;
      *value = b ? 1 : 0;
      return true;
    case V4L2_CID_POWER_LINE_FREQUENCY: {
      uint8_t hz = _cam->antiFlicker();
      *value = (hz == 50) ? 1 : (hz == 60) ? 2 : 0;
      return true;
    }
    case V4L2_CID_SHARPNESS:
      *value = _cam->sharpness();
      return true;
    case V4L2_CID_EXPOSURE_ABSOLUTE:
      return _cam->getExposureTime(value);
    case V4L2_CID_FOCUS_ABSOLUTE:
      *value = _cam->afPosition();
      return true;
    case V4L2_CID_JPEG_COMPRESSION_QUALITY:
      *value = _cam->jpegQuality();
      return true;
    case V4L2_CID_TEST_PATTERN:
      *value = _cam->testPattern() ? 1 : 0;
      return true;
    case V4L2_CID_CAMERA_AE_LEVEL:
      *value = _cam->aeTarget();
      return true;
    default:
      return false;
  }
}

void ESP32P4_V4l2::listCtrls() const {
  Serial.println("V4L2: controls (v4l2-ctl --list-ctrls)");
  for (int i = 0; i < kCtrlN; i++) {
    int32_t v = 0;
    (void)getCtrl(kCtrls[i].id, &v);
    Serial.printf("  %-28s 0x%08x (%s) : min=%d max=%d step=%d default=%d value=%d\n",
                  kCtrls[i].ctl, (unsigned)kCtrls[i].id,
                  kCtrls[i].type == V4L2_CTRL_TYPE_BOOLEAN ? "bool" : "int", kCtrls[i].minv,
                  kCtrls[i].maxv, kCtrls[i].step, kCtrls[i].defv, (int)v);
  }
}

void ESP32P4_V4l2::listFormats() const {
  if (!_cam) return;
  Serial.printf("V4L2: formats on %s (v4l2-ctl --list-formats)\n", _path);
  uint32_t idx = 0;
  for (int i = 0; i < kFmtN; i++) {
    if (!_cam->supportsFormat(kFmts[i].pix)) continue;
    char fcc[5];
    fourcc_str(kFmts[i].fourcc, fcc);
    Serial.printf("  [%u]: '%s' (%s)%s\n", (unsigned)idx, fcc, kFmts[i].name,
                  kFmts[i].pix == _cam->format() ? " *" : "");
    idx++;
  }
  Serial.printf("  size: %ux%u  fps=%u\n", _cam->width(), _cam->height(),
                (unsigned)_cam->modeFps());
}

static void skip_ws(const char *&s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
}

int ESP32P4_V4l2::ctl(const char *cmdline) {
  if (!cmdline) return -1;
  const char *s = cmdline;
  skip_ws(s);
  if (!strncmp(s, "v4l2-ctl", 8)) {
    s += 8;
    skip_ws(s);
  }
  if (strstr(s, "--list-ctrls")) {
    listCtrls();
    return 0;
  }
  if (strstr(s, "--list-formats")) {
    listFormats();
    return 0;
  }
  if (strstr(s, "--get-fmt-video") || strstr(s, "--all")) {
    listFormats();
    if (strstr(s, "--all")) listCtrls();
    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (fillFmt(&fmt)) {
      char fcc[5];
      fourcc_str(fmt.fmt.pix.pixelformat, fcc);
      Serial.printf(
          "Format Video Capture:\n\tWidth/Height      : %u/%u\n\tPixel Format      : '%s'\n",
          (unsigned)fmt.fmt.pix.width, (unsigned)fmt.fmt.pix.height, fcc);
    }
    return 0;
  }
  const char *p = strstr(s, "--set-fmt-video=");
  if (p) {
    p += 16;
    const char *pix = strstr(p, "pixelformat=");
    if (pix) {
      pix += 12;
      char tok[8]{};
      int n = 0;
      while (pix[n] && pix[n] != ',' && pix[n] != ' ' && n < 7) {
        tok[n] = pix[n];
        n++;
      }
      uint32_t fcc = 0;
      if (n == 4)
        fcc = (uint32_t)tok[0] | ((uint32_t)tok[1] << 8) | ((uint32_t)tok[2] << 16) |
              ((uint32_t)tok[3] << 24);
      esp32p4_cam_pixformat_t fmt;
      if (!fourccToPix(fcc, &fmt) || !_cam->setFormat(fmt)) {
        Serial.println("V4L2: set-fmt-video failed");
        return -1;
      }
      Serial.printf("V4L2: pixelformat %s\n", _cam->formatName());
    }
    return 0;
  }
  p = strstr(s, "--set-ctrl");
  if (p) {
    const char *q = strchr(p, ' ');
    if (!q) q = strchr(p, '=');
    if (!q) return -1;
    skip_ws(q);
    char buf[160];
    strncpy(buf, q, sizeof(buf) - 1);
    char *save = nullptr;
    for (char *tok = strtok_r(buf, ", ", &save); tok; tok = strtok_r(nullptr, ", ", &save)) {
      char *eq = strchr(tok, '=');
      if (!eq) continue;
      *eq = 0;
      const CtrlDesc *d = findCtrlName(tok);
      if (!d) {
        Serial.printf("V4L2: unknown ctrl %s\n", tok);
        continue;
      }
      int32_t v = (int32_t)atoi(eq + 1);
      if (!setCtrl(d->id, v)) Serial.printf("V4L2: set %s failed\n", tok);
      else Serial.printf("V4L2: %s=%d\n", tok, (int)v);
    }
    return 0;
  }
  p = strstr(s, "--get-ctrl");
  if (p) {
    const char *q = strchr(p, ' ');
    if (!q) return -1;
    skip_ws(q);
    char buf[160];
    strncpy(buf, q, sizeof(buf) - 1);
    char *save = nullptr;
    for (char *tok = strtok_r(buf, ", ", &save); tok; tok = strtok_r(nullptr, ", ", &save)) {
      const CtrlDesc *d = findCtrlName(tok);
      int32_t v = 0;
      if (!d || !getCtrl(d->id, &v)) Serial.printf("%s: n/a\n", tok);
      else Serial.printf("%s: %d\n", tok, (int)v);
    }
    return 0;
  }
  p = strstr(s, "--stream-count=");
  if (p) {
    int n = atoi(p + 15);
    if (n < 1) n = 1;
    for (int i = 0; i < n; i++) {
      camera_fb_t *fb = dqbuf(_dq_ms);
      if (!fb) {
        Serial.println("V4L2: DQBUF timeout");
        return -1;
      }
      Serial.printf("V4L2: frame %d  %ux%u  %u bytes  %s\n", i, fb->width, fb->height,
                    (unsigned)fb->len, esp32p4_pixformat_name(fb->format));
      qbuf(fb);
    }
    return 0;
  }
  Serial.println(
      "V4L2: try --list-ctrls | --list-formats | --set-ctrl name=val | --get-fmt-video");
  return -1;
}

camera_fb_t *ESP32P4_V4l2::dqbuf(uint32_t timeout_ms) {
  if (!_cam) return nullptr;
  return _cam->capture(timeout_ms ? timeout_ms : _dq_ms);
}

void ESP32P4_V4l2::qbuf(camera_fb_t *fb) {
  if (_cam && fb) _cam->release(fb);
}

#else /* !ESP32P4_HAS_V4L2 */

const char *ESP32P4_V4l2::defaultPath(esp32p4_cam_bus_t) { return "/dev/video0"; }
const char *ESP32P4_V4l2::defaultPath(ESP32P4_Camera *) { return "/dev/video0"; }
uint32_t ESP32P4_V4l2::pixToFourcc(esp32p4_cam_pixformat_t fmt) {
  return esp32p4_pixformat_fourcc(fmt);
}
bool ESP32P4_V4l2::fourccToPix(uint32_t, esp32p4_cam_pixformat_t *) { return false; }
bool ESP32P4_V4l2::begin(ESP32P4_Camera *, const char *) {
  Serial.println("V4L2: linux/videodev2.h not in this core");
  return false;
}
void ESP32P4_V4l2::end() {}
int ESP32P4_V4l2::ioctl(unsigned long, void *) { return -1; }
bool ESP32P4_V4l2::setCtrl(uint32_t, int32_t) { return false; }
bool ESP32P4_V4l2::getCtrl(uint32_t, int32_t *) const { return false; }
bool ESP32P4_V4l2::queryCtrl(uint32_t, char *, int32_t *, int32_t *, int32_t *, int32_t *) const {
  return false;
}
void ESP32P4_V4l2::listCtrls() const {}
void ESP32P4_V4l2::listFormats() const {}
int ESP32P4_V4l2::ctl(const char *) { return -1; }
camera_fb_t *ESP32P4_V4l2::dqbuf(uint32_t) { return nullptr; }
void ESP32P4_V4l2::qbuf(camera_fb_t *) {}
void *ESP32P4_V4l2::mmap(size_t, off_t) { return (void *)-1; }
int ESP32P4_V4l2::munmap(void *, size_t) { return -1; }

#endif
