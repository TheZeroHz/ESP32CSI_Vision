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
  ESP32P4_SENSOR_IMX477,
  ESP32P4_SENSOR_IMX335,
  ESP32P4_SENSOR_IMX415,
  ESP32P4_SENSOR_GC2083,
  ESP32P4_SENSOR_GC2093,
  /* Tier C — detect stubs */
  ESP32P4_SENSOR_OV7251,
  ESP32P4_SENSOR_IMX296,
  ESP32P4_SENSOR_IMX462,
  ESP32P4_SENSOR_ARDUCAM_IMX500,
  ESP32P4_SENSOR_COUNT
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
  ESP32P4_PIXFORMAT_RGB565 = 0,
  ESP32P4_PIXFORMAT_RAW10,
  ESP32P4_PIXFORMAT_RAW8,
};

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
  /** Framebuffer after ISP. Use RGB565. Sensor MIPI RAW is not this field. */
  esp32p4_cam_pixformat_t pixel_format;
  int lane_bit_rate_mbps;
  esp32p4_cam_sensor_t sensor;
  bool test_pattern;
  uint8_t fb_count;
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

class ESP32P4_Camera {
 public:
  bool begin(esp32p4_board_t board = ESP32P4_BOARD_GUITION_M3);
  bool begin(const esp32p4_cam_config_t &cfg);
  void end();
  camera_fb_t *capture(uint32_t timeout_ms = 2000);
  void release(camera_fb_t *fb);
  bool setTestPattern(bool enable);
  bool testPattern() const { return _cfg.test_pattern; }
  bool setHMirror(bool enable);
  bool setVFlip(bool enable);
  bool setAEC(bool enable);
  bool setAGC(bool enable);
  bool setExposure(uint16_t lines);
  bool setGain(uint16_t gain);
  bool setGainCeiling(uint16_t ceiling);

  bool getHMirror(bool *out) const;
  bool getVFlip(bool *out) const;
  bool getAEC(bool *out) const;
  bool getAGC(bool *out) const;
  bool getExposure(uint16_t *lines) const;
  bool getGain(uint16_t *gain) const;
  bool getGainCeiling(uint16_t *ceiling) const;

  uint16_t width() const { return _w; }
  uint16_t height() const { return _h; }
  int sensorAddress() const { return _addr; }
  bool detected() const { return _addr > 0; }
  esp32p4_cam_sensor_t sensorType() const { return _sensor; }
  const char *sensorName() const;
  uint32_t newTransCount() const;
  uint32_t doneCount() const;
  uint8_t fbCount() const { return _fb_n; }
  bool psramOk() const;
  uint8_t dataLanes() const { return _lanes; }

 private:
  bool probe_sensor();
  bool init_sensor();
  bool init_mipi_ldo();
  bool init_csi_isp();
  bool start_sensor_stream();
  bool alloc_fbs();
  bool sync_isp_bayer_for_flip();

  esp32p4_cam_config_t _cfg{};
  esp32p4_cam_sensor_t _sensor = ESP32P4_SENSOR_AUTO;
  const void *_ops = nullptr;  // esp32p4_cam_sensor_ops_t*
  bool _raw8 = false;
  bool _use_isp = true;
  uint8_t _lanes = 2;
  uint8_t _bayer = 0;
  int _addr = 0;
  uint16_t _w = 0;
  uint16_t _h = 0;
  uint8_t _fb_n = 0;
  camera_fb_t _fb[ESP32P4_CAM_FB_MAX]{};
  size_t _fb_cap = 0;
  void *_cam = nullptr;
  void *_isp = nullptr;
  void *_ldo = nullptr;
  bool _started = false;
};

using ESP32P4_CSI_Camera = ESP32P4_Camera;
