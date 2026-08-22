#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/sensors/ov5647_sensor.h"
#include "cam/esp32p4_sccb.h"

static const uint8_t kAddrs[] = {0x36, 0};

static bool detect(uint8_t *addr7_out) { return ov5647_detect(addr7_out); }

static bool stream_on(uint8_t addr7) { return ov5647_stream_restart(addr7); }
static bool stream_off(uint8_t addr7) {
  // Soft standby via stream bit if available — restart handles on path.
  (void)addr7;
  return true;
}

static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  esp32p4_cam_mode_t m{};
  m.bayer = ESP32P4_BAYER_GBRG;
  m.lanes = 2;
  bool ok = false;
  const bool want1080 = (want == ESP32P4_FRAMESIZE_AUTO || want == ESP32P4_FRAMESIZE_1080P);
  const bool want800 = (want == ESP32P4_FRAMESIZE_800X640);
  if (want1080 && !want800) {
    m.name = "OV5647 1920x1080 RAW10";
    m.width = 1920;
    m.height = 1080;
    m.in_fmt = ESP32P4_CAM_IN_RAW10;
    m.lane_mbps = 500;
    m.framesize_tag = ESP32P4_FRAMESIZE_1080P;
    m.fps = 30;
    ok = ov5647_configure_1920x1080_raw10(addr7);
  }
  if (!ok) {
    m.name = "OV5647 800x640 RAW8";
    m.width = 800;
    m.height = 640;
    m.in_fmt = ESP32P4_CAM_IN_RAW8;
    m.lane_mbps = 200;
    m.framesize_tag = ESP32P4_FRAMESIZE_800X640;
    m.fps = 50;
    ok = ov5647_configure_800x640_raw8(addr7);
  }
  if (ok && mode_out) *mode_out = m;
  return ok;
}

static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_OV5647,
    "OV5647",
    ESP32P4_CAM_SUPPORT_FULL,
    kAddrs,
    detect,
    configure,
    stream_on,
    stream_off,
    ov5647_set_hmirror,
    ov5647_set_vflip,
    ov5647_get_hmirror,
    ov5647_get_vflip,
    ov5647_set_aec,
    ov5647_set_agc,
    ov5647_get_aec,
    ov5647_get_agc,
    ov5647_set_exposure,
    ov5647_get_exposure,
    ov5647_set_gain,
    ov5647_get_gain,
    ov5647_set_gainceiling,
    ov5647_get_gainceiling,
    ov5647_set_test_pattern,
};

const esp32p4_cam_sensor_ops_t *ov5647_sensor_ops(void) { return &kOps; }
