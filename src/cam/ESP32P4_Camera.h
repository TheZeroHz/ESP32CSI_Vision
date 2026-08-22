#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
#error "ESP32CSI_Vision: this build targets ESP32-P4 MIPI CSI (other CSI SoCs later)."
#endif

/**
 * MIPI-CSI sensors supported by ESP32CSI_Vision (first-party drivers, no ESP_Video).
 * AUTO probes the registry in order. DETECT_ONLY IDs are reported when chip-ID matches
 * but a full mode table is not yet wired.
 */
enum esp32p4_cam_sensor_t {
  ESP32P4_SENSOR_AUTO = 0,
  /* Tier A — Espressif MIPI parity */
  ESP32P4_SENSOR_SC2336,
  ESP32P4_SENSOR_OV5647,
  ESP32P4_SENSOR_OV5645,
  ESP32P4_SENSOR_OV2710,
  ESP32P4_SENSOR_OV9281,
  ESP32P4_SENSOR_SC202CS,
  ESP32P4_SENSOR_SC1346,
  ESP32P4_SENSOR_SC030IOT,
  ESP32P4_SENSOR_SC035HGS,
  ESP32P4_SENSOR_OS02N10,
  ESP32P4_SENSOR_OS04C10,
  ESP32P4_SENSOR_GC2145,
  ESP32P4_SENSOR_STI2250,
  ESP32P4_SENSOR_SC121AT,
  ESP32P4_SENSOR_MIRA220,
  /* Tier B — market / Pi-class (beyond Espressif esp_cam_sensor) */
  ESP32P4_SENSOR_IMX708,
  ESP32P4_SENSOR_IMX219,
  ESP32P4_SENSOR_IMX477,  // Pi HQ Cam / IMX378 — 1920x1080 RAW10
  ESP32P4_SENSOR_IMX335,
  ESP32P4_SENSOR_IMX415,
  ESP32P4_SENSOR_GC2083,
  ESP32P4_SENSOR_GC2093,
  /* Tier C — detect stubs */
  ESP32P4_SENSOR_OV7251,
  ESP32P4_SENSOR_IMX296,
  ESP32P4_SENSOR_IMX462,  // STARVIS 1080p — IMX290/327/462
  ESP32P4_SENSOR_ARDUCAM_IMX500,
  /* Appended so existing enum values stay stable */
  ESP32P4_SENSOR_SC2331,
  ESP32P4_SENSOR_GC2607,
  ESP32P4_SENSOR_OV5640,
  ESP32P4_SENSOR_LT6911,
  ESP32P4_SENSOR_OV2640,  // DVP VGA RGB565
  ESP32P4_SENSOR_SP0A39,  // SPI 1-bit gray VGA
  ESP32P4_SENSOR_COUNT
};

/** Capture bus. CSI is the default; DVP / SPI / USB-host UVC share camera_fb_t. */
enum esp32p4_cam_bus_t {
  ESP32P4_CAM_BUS_CSI = 0,
  ESP32P4_CAM_BUS_DVP,
  ESP32P4_CAM_BUS_SPI,
  ESP32P4_CAM_BUS_UVC_HOST,
};

struct esp32p4_cam_dvp_pins_t {
  int data[16];
  int vsync;
  int de;
  int pclk;
  int xclk;
  uint8_t data_width;  // 8 or 16; 0 → 8
};

struct esp32p4_cam_spi_pins_t {
  int cs;
  int sclk;
  int d0;
  int d1;
  int d2;
  int d3;
  int xclk;
  uint8_t spi_port;  // spi_host_device_t; 0 = SPI2_HOST default
  uint8_t io_mode;   // 0=1-bit, 1=2-bit, 2=4-bit
  uint8_t intf;      // 0=SPI, 1=PARLIO
};

struct esp32p4_cam_uvc_host_t {
  uint16_t vid;       // 0 = any
  uint16_t pid;       // 0 = any
  uint8_t dev_addr;   // 0 = any
  uint8_t format;     // 0=device default, 1=MJPEG, 2=YUY2 (uvc_host_stream_format)
  uint16_t width;     // 0 → 640
  uint16_t height;    // 0 → 480
  float fps;          // 0 → device default
};

enum esp32p4_cam_framesize_t {
  ESP32P4_FRAMESIZE_AUTO = 0,
  ESP32P4_FRAMESIZE_VGA,          // 640x480
  ESP32P4_FRAMESIZE_800X640,      // OV5647 RAW8 binning
  ESP32P4_FRAMESIZE_SVGA,         // 800x600
  ESP32P4_FRAMESIZE_HD,           // 1280x720
  ESP32P4_FRAMESIZE_SXGA,         // 1280x960
  ESP32P4_FRAMESIZE_1080P,        // 1920x1080
  ESP32P4_FRAMESIZE_2304X1296,    // IMX708 binned
  ESP32P4_FRAMESIZE_QXGA,         // 1600x1200
  ESP32P4_FRAMESIZE_5MP,          // ~2592x1944 (YUV sensors / future RAW)
};

enum esp32p4_cam_pixformat_t {
  ESP32P4_PIXFORMAT_RGB565 = 0,  // default; MJPEG / CV / face
  ESP32P4_PIXFORMAT_RAW10,       // ISP bypass, unpacked 16-bit
  ESP32P4_PIXFORMAT_RAW8,        // ISP bypass
  ESP32P4_PIXFORMAT_RGB888,
  ESP32P4_PIXFORMAT_YUV422,  // UYVY
  ESP32P4_PIXFORMAT_YUV420,
  ESP32P4_PIXFORMAT_GRAY8,  // Y plane of ISP YUV420
  ESP32P4_PIXFORMAT_JPEG,   // HW JPEG capture (CSI stays RGB565; fb is JFIF)
  /* Appended — existing values stay stable */
  ESP32P4_PIXFORMAT_YUYV,   // YUYV packed (ISP UYVY swapped; UVC host YUY2 native)
  ESP32P4_PIXFORMAT_RAW12,  // ISP bypass, unpacked 16-bit (no sensor table yet)
};

/** JPEG output chroma. AUTO follows the input (RGB→422, RGB888→444, YUV as-is). */
enum esp32p4_jpeg_chroma_t {
  ESP32P4_JPEG_CHROMA_AUTO = 0,
  ESP32P4_JPEG_CHROMA_YUV444,
  ESP32P4_JPEG_CHROMA_YUV422,
  ESP32P4_JPEG_CHROMA_YUV420,
};

class ESP32P4_Jpeg;
class ESP32P4_Isp;
class ESP32P4_Camera;

enum esp32p4_board_t {
  ESP32P4_BOARD_GUITION_M3 = 0,
  ESP32P4_BOARD_WAVESHARE_NANO,
  ESP32P4_BOARD_FUNCTION_EV,
  ESP32P4_BOARD_CUSTOM,
};

#ifndef ESP32P4_CAM_FB_MAX
#define ESP32P4_CAM_FB_MAX 3
#endif

struct esp32p4_cam_config_t {
  int sda;
  int scl;
  TwoWire *wire;  // nullptr / &Wire = I2C0; &Wire1 = second bus (e.g. T-Display P4 camera)
  int xclk;
  int pwdn;
  int reset;
  uint32_t xclk_hz;
  int i2c_addr;
  int ldo_chan;
  int ldo_mv;
  esp32p4_cam_framesize_t frame_size;
  /** Framebuffer format after ISP (or JPEG / RAW). Default RGB565. */
  esp32p4_cam_pixformat_t pixel_format;
  int lane_bit_rate_mbps;
  esp32p4_cam_sensor_t sensor;
  bool test_pattern;
  uint8_t fb_count;
  /**
   * MIPI CSI host id. ESP32-P4 has one CSI controller — must be 0.
   * A second CSI object is rejected. Two objects on *different* buses
   * (CSI+DVP, CSI+SPI, CSI+UVC_HOST, …) can capture at the same time.
   */
  uint8_t csi_id;
  /** Capture bus. Default CSI — existing sketches unchanged. */
  esp32p4_cam_bus_t bus;
  esp32p4_cam_dvp_pins_t dvp;
  esp32p4_cam_spi_pins_t spi;
  esp32p4_cam_uvc_host_t uvc;
};

struct camera_fb_t {
  uint8_t *buf;
  size_t len;
  uint16_t width;
  uint16_t height;
  esp32p4_cam_pixformat_t format;
  uint32_t timestamp_us;
};

esp32p4_cam_config_t esp32p4_cam_config_default();
esp32p4_cam_config_t esp32p4_cam_config_board(esp32p4_board_t board);
/** Two buses can stream at once (not two CSI). Two SPI OK on different spi_port. */
bool esp32p4_cam_dual_ok(esp32p4_cam_bus_t a, esp32p4_cam_bus_t b);
const char *esp32p4_cam_dual_why(esp32p4_cam_bus_t a, esp32p4_cam_bus_t b);
bool esp32p4_cam_dual_ok(const ESP32P4_Camera &a, const ESP32P4_Camera &b);
const char *esp32p4_cam_dual_why(const ESP32P4_Camera &a, const ESP32P4_Camera &b);
const char *esp32p4_pixformat_name(esp32p4_cam_pixformat_t fmt);
size_t esp32p4_pixformat_fb_bytes(esp32p4_cam_pixformat_t fmt, uint16_t w, uint16_t h);
/** CSI/ISP backing format: JPEG capture is encoded from RGB565. */
esp32p4_cam_pixformat_t esp32p4_pixformat_pipe(esp32p4_cam_pixformat_t fmt);
/** V4L2-style fourcc (no V4L2 dependency). JPEG is 'JPEG', RGB565 is 'RGBP'. */
uint32_t esp32p4_pixformat_fourcc(esp32p4_cam_pixformat_t fmt);

class ESP32P4_Camera {
 public:
  ~ESP32P4_Camera() { end(); }
  bool begin(esp32p4_board_t board = ESP32P4_BOARD_GUITION_M3);
  bool begin(const esp32p4_cam_config_t &cfg);
  void end();
  camera_fb_t *capture(uint32_t timeout_ms = 2000);
  void release(camera_fb_t *fb);

  bool startCapture();
  bool stopCapture();
  bool isCaptureStarted() const { return _started; }

  bool setFormat(esp32p4_cam_pixformat_t fmt);
  esp32p4_cam_pixformat_t format() const { return _cfg.pixel_format; }
  const char *formatName() const { return esp32p4_pixformat_name(_cfg.pixel_format); }
  /** True if this sensor/ISP path can produce `fmt` (JPEG always, via RGB565). */
  bool supportsFormat(esp32p4_cam_pixformat_t fmt) const;

  bool setTestPattern(bool enable);
  bool testPattern() const { return _cfg.test_pattern; }
  bool setHMirror(bool enable);
  bool setVFlip(bool enable);
  bool setAEC(bool enable);
  bool setAGC(bool enable);
  bool setExposure(uint16_t lines);
  bool setGain(uint16_t gain);
  bool setGainCeiling(uint16_t ceiling);
  /** V4L2-style absolute exposure; unit is 100 µs (Arduino ESP_Video). Uses mode fps. */
  bool setExposureTime(int32_t units_100us);
  bool getExposureTime(int32_t *units_100us) const;
  uint8_t modeFps() const { return _fps; }
  uint32_t lineTimeUs() const { return _line_us; }
  bool setAeTarget(uint8_t luma);
  uint8_t aeTarget() const;
  /** EV bias in 1/2-stop units (−4..+4) on IPA AGC. SmartAe uses the same scale. */
  bool setAeEvBias(int half_stops);
  int aeEvBias() const;
  /** AGC anti-flicker: 0 = off (profile), 50 or 60 Hz snaps exposure to 1/(2f). */
  bool setAntiFlicker(uint8_t hz);
  uint8_t antiFlicker() const;
  bool setJpegQuality(uint8_t q);
  uint8_t jpegQuality() const { return _jpeg_quality; }
  bool setJpegChroma(esp32p4_jpeg_chroma_t c);
  esp32p4_jpeg_chroma_t jpegChroma() const { return _jpeg_chroma; }

  bool setBrightness(int8_t v);
  bool setContrast(uint8_t v);
  bool setSaturation(uint8_t v);
  bool setHue(uint16_t deg);
  bool setAwb(bool on);
  bool setIspAe(bool on);
  bool setRedBalance(int32_t v1024);
  bool setBlueBalance(int32_t v1024);
  bool setSharpness(uint8_t v);
  bool setDenoise(uint8_t v);
  bool ispReady() const;
  float ispLuma() const;
  float ispEnvLuma() const;
  int8_t brightness() const;
  uint8_t contrast() const;
  uint8_t saturation() const;
  uint16_t hue() const;
  uint8_t sharpness() const;
  uint8_t denoise() const;

  bool getHMirror(bool *out) const;
  bool getVFlip(bool *out) const;
  bool getAEC(bool *out) const;
  bool getAGC(bool *out) const;
  bool getExposure(uint16_t *lines) const;
  bool getGain(uint16_t *gain) const;
  bool getGainCeiling(uint16_t *ceiling) const;

  bool setSensorGain(int32_t gain) { return setGain((uint16_t)gain); }
  bool setSensorExposure(int32_t exposure) { return setExposure((uint16_t)exposure); }
  bool setSensorExposureTime(int32_t exposure_100us) { return setExposureTime(exposure_100us); }
  bool setSensorAETargetLevel(int32_t target) {
    return setAeTarget((uint8_t)(target < 0 ? 0 : (target > 255 ? 255 : target)));
  }
  bool setSensorJPEGQuality(int32_t quality) {
    return setJpegQuality((uint8_t)(quality < 1 ? 1 : (quality > 100 ? 100 : quality)));
  }
  bool setSensorVFlip(bool vflip) { return setVFlip(vflip); }
  bool setSensorHFlip(bool hflip) { return setHMirror(hflip); }
  bool setSensorTestPattern(bool test_pattern) { return setTestPattern(test_pattern); }
  bool setSensorBrightness(int32_t v) {
    int8_t b = (int8_t)(v < -128 ? -128 : (v > 127 ? 127 : v));
    return setBrightness(b);
  }
  bool setSensorContrast(int32_t v) {
    return setContrast((uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)));
  }
  bool setSensorSaturation(int32_t v) {
    return setSaturation((uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)));
  }
  bool setSensorHue(int32_t deg) { return setHue((uint16_t)((deg % 360 + 360) % 360)); }
  bool setSensorRedBalance(int32_t v1024) { return setRedBalance(v1024); }
  bool setSensorBlueBalance(int32_t v1024) { return setBlueBalance(v1024); }
  bool setSensorSharpness(int32_t v) {
    return setSharpness((uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)));
  }
  bool setSensorDenoise(int32_t v) {
    return setDenoise((uint8_t)(v < 0 ? 0 : (v > 8 ? 8 : v)));
  }
  bool setSensorAwb(bool on) { return setAwb(on); }
  bool setSensorAntiFlicker(int32_t hz) {
    return setAntiFlicker((uint8_t)(hz < 0 ? 0 : (hz > 255 ? 255 : hz)));
  }

  /** DW9714 VCM (OV5647 AF modules). Position 0–1023. */
  bool afBegin();
  bool afPresent() const { return _af_ok; }
  bool setAfPosition(uint16_t pos);
  uint16_t afPosition() const { return _af_pos; }
  bool setSensorAfPosition(int32_t pos) {
    return setAfPosition((uint16_t)(pos < 0 ? 0 : (pos > 1023 ? 1023 : pos)));
  }
  /**
   * Two-level contrast AF (ISP definition stats + DW9714). Blocking ~0.5 s.
   * max_pos 0 → IPA default (OV5647 500, else 1023). Call after begin().
   */
  bool afScan(uint16_t min_pos = 0, uint16_t max_pos = 0);
  uint32_t afScore();

  uint16_t width() const { return _w; }
  uint16_t height() const { return _h; }
  int sensorAddress() const { return _addr; }
  bool detected() const { return _addr > 0; }
  esp32p4_cam_sensor_t sensorType() const { return _sensor; }
  const char *sensorName() const;
  uint32_t newTransCount() const;
  uint32_t doneCount() const;
  uint32_t dropCount() const;
  uint8_t fbCount() const { return _fb_n; }
  /** DMA/PSRAM slot for V4L2 MMAP (index < fbCount()). */
  uint8_t *fbBuf(uint8_t index) const;
  size_t fbBufBytes() const { return _fb_cap; }
  uint8_t spiPort() const { return _cfg.spi.spi_port; }
  /** First-party ESP_CAM_SENSOR_IOC_* (no esp_cam_sensor link). */
  bool sensorIoctl(uint32_t cmd, void *arg, size_t size);
  bool psramOk() const;
  uint8_t dataLanes() const { return _lanes; }
  ESP32P4_Isp *isp() { return _isp_pipe; }
  esp32p4_cam_bus_t bus() const { return _bus; }
  const char *busName() const;

 private:
  friend struct ESP32P4_CamFbAccess;
  bool probe_sensor();
  bool init_sensor();
  bool init_mipi_ldo();
  bool init_csi_isp();
  bool init_dvp();
  bool init_spi();
  bool init_uvc_host();
  bool start_pipeline();
  bool start_sensor_stream();
  bool alloc_fbs();
  void free_fbs();
  void teardown_pipeline();
  void teardown_uvc_host();
  bool claim_bus();
  void release_bus();
  bool sync_isp_bayer_for_flip();
  esp32p4_cam_pixformat_t resolve_format(esp32p4_cam_pixformat_t want) const;
  bool ensure_jpeg_encoder();
  void free_jpeg_encoder();

  esp32p4_cam_config_t _cfg{};
  esp32p4_cam_sensor_t _sensor = ESP32P4_SENSOR_AUTO;
  const void *_ops = nullptr;  // esp32p4_cam_sensor_ops_t*
  bool _raw8 = false;
  bool _use_isp = true;
  uint8_t _in_fmt = 1;  // esp32p4_cam_in_fmt_t; RAW10 default
  uint8_t _lanes = 2;
  uint8_t _bayer = 0;
  int _addr = 0;
  uint16_t _w = 0;
  uint16_t _h = 0;
  uint8_t _fps = 25;
  uint32_t _line_us = 30;
  bool _af_ok = false;
  uint16_t _af_pos = 0;
  uint8_t _fb_n = 0;
  camera_fb_t _fb[ESP32P4_CAM_FB_MAX]{};
  size_t _fb_cap = 0;
  void *_cam = nullptr;
  void *_isp = nullptr;
  ESP32P4_Isp *_isp_pipe = nullptr;
  void *_ldo = nullptr;
  bool _started = false;
  uint8_t _jpeg_quality = 45;
  esp32p4_jpeg_chroma_t _jpeg_chroma = ESP32P4_JPEG_CHROMA_AUTO;
  ESP32P4_Jpeg *_jpeg_enc = nullptr;
  uint8_t *_jpeg_scratch = nullptr;
  size_t _jpeg_scratch_cap = 0;
  esp32p4_cam_bus_t _bus = ESP32P4_CAM_BUS_CSI;
  void *_pool = nullptr;       // CamFbPool*
  void *_uvc_stream = nullptr; // uvc_host_stream_hdl_t
};

/** RAII frame (beats Arduino ESP_Video's buffer wrapper: same capture() path). */
class ESP32P4_Frame {
 public:
  ESP32P4_Frame() = default;
  explicit ESP32P4_Frame(ESP32P4_Camera &cam, uint32_t timeout_ms = 2000)
      : _cam(&cam), _fb(cam.capture(timeout_ms)) {}
  ~ESP32P4_Frame() { release(); }
  ESP32P4_Frame(const ESP32P4_Frame &) = delete;
  ESP32P4_Frame &operator=(const ESP32P4_Frame &) = delete;
  ESP32P4_Frame(ESP32P4_Frame &&o) noexcept : _cam(o._cam), _fb(o._fb) {
    o._fb = nullptr;
    o._cam = nullptr;
  }
  ESP32P4_Frame &operator=(ESP32P4_Frame &&o) noexcept {
    if (this != &o) {
      release();
      _cam = o._cam;
      _fb = o._fb;
      o._fb = nullptr;
      o._cam = nullptr;
    }
    return *this;
  }
  explicit operator bool() const { return _fb != nullptr; }
  bool valid() const { return _fb != nullptr; }
  camera_fb_t *get() const { return _fb; }
  camera_fb_t *operator->() const { return _fb; }
  uint8_t *data() const { return _fb ? _fb->buf : nullptr; }
  size_t size() const { return _fb ? _fb->len : 0; }
  uint16_t width() const { return _fb ? _fb->width : 0; }
  uint16_t height() const { return _fb ? _fb->height : 0; }
  esp32p4_cam_pixformat_t format() const {
    return _fb ? _fb->format : ESP32P4_PIXFORMAT_RGB565;
  }
  void release() {
    if (_cam && _fb) _cam->release(_fb);
    _fb = nullptr;
  }

 private:
  ESP32P4_Camera *_cam = nullptr;
  camera_fb_t *_fb = nullptr;
};

using ESP32P4_CSI_Camera = ESP32P4_Camera;
