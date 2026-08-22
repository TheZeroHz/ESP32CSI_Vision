#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "cam/ESP32P4_Camera.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint16_t reg; uint8_t val; } esp32p4_reg8_t;
typedef struct { uint8_t reg; uint8_t val; } esp32p4_reg8a8_t;
typedef enum { ESP32P4_BAYER_RGGB=0, ESP32P4_BAYER_GRBG, ESP32P4_BAYER_GBRG, ESP32P4_BAYER_BGGR, ESP32P4_BAYER_NONE } esp32p4_cam_bayer_t;
typedef enum { ESP32P4_CAM_IN_RAW8=0, ESP32P4_CAM_IN_RAW10, ESP32P4_CAM_IN_RGB565, ESP32P4_CAM_IN_YUV422, ESP32P4_CAM_IN_GRAY8 } esp32p4_cam_in_fmt_t;
typedef enum { ESP32P4_CAM_SUPPORT_FULL=0, ESP32P4_CAM_SUPPORT_EXPERIMENTAL, ESP32P4_CAM_SUPPORT_DETECT_ONLY } esp32p4_cam_support_t;
typedef struct {
  const char *name; uint16_t width; uint16_t height; uint8_t lanes; int lane_mbps;
  esp32p4_cam_in_fmt_t in_fmt; esp32p4_cam_bayer_t bayer; esp32p4_cam_framesize_t framesize_tag;
  const esp32p4_reg8_t *regs; size_t regs_count;
  uint8_t fps; /* 0 = unknown; used for EXPOSURE_ABSOLUTE line time */
  const void *spi_frame_info; /* esp_cam_sensor_spi_frame_info* for SPI sensors */
} esp32p4_cam_mode_t;
typedef struct esp32p4_cam_sensor_ops esp32p4_cam_sensor_ops_t;
struct esp32p4_cam_sensor_ops {
  esp32p4_cam_sensor_t id; const char *name; esp32p4_cam_support_t support; const uint8_t *addrs;
  bool (*detect)(uint8_t *addr7_out);
  bool (*configure)(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out);
  bool (*stream_on)(uint8_t addr7); bool (*stream_off)(uint8_t addr7);
  bool (*set_hmirror)(uint8_t,bool); bool (*set_vflip)(uint8_t,bool);
  bool (*get_hmirror)(uint8_t,bool*); bool (*get_vflip)(uint8_t,bool*);
  bool (*set_aec)(uint8_t,bool); bool (*set_agc)(uint8_t,bool);
  bool (*get_aec)(uint8_t,bool*); bool (*get_agc)(uint8_t,bool*);
  bool (*set_exposure)(uint8_t,uint16_t); bool (*get_exposure)(uint8_t,uint16_t*);
  bool (*set_gain)(uint8_t,uint16_t); bool (*get_gain)(uint8_t,uint16_t*);
  bool (*set_gainceiling)(uint8_t,uint16_t); bool (*get_gainceiling)(uint8_t,uint16_t*);
  bool (*set_test_pattern)(uint8_t,bool);
  esp32p4_cam_bus_t bus; /* 0 = CSI; DVP/SPI sensors set this */
};
const esp32p4_cam_sensor_ops_t *const *esp32p4_cam_sensor_registry(void);
size_t esp32p4_cam_sensor_registry_count(void);
const esp32p4_cam_sensor_ops_t *esp32p4_cam_sensor_find(esp32p4_cam_sensor_t id);
const esp32p4_cam_sensor_ops_t *esp32p4_cam_sensor_probe(esp32p4_cam_sensor_t prefer, uint8_t *addr7_out);
const esp32p4_cam_sensor_ops_t *esp32p4_cam_sensor_probe_bus(esp32p4_cam_sensor_t prefer, uint8_t *addr7_out, esp32p4_cam_bus_t bus);
bool esp32p4_cam_write_reg8_table(uint8_t addr7, const esp32p4_reg8_t *regs, size_t n);
size_t esp32p4_cam_reg8_count(const esp32p4_reg8_t *regs);
bool esp32p4_cam_write_reg8a8_table(uint8_t addr7, const esp32p4_reg8a8_t *regs, size_t n);
#ifdef __cplusplus
}
#endif
