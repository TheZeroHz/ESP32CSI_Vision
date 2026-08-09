#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/sensors/imx708_sensor.h"

static const uint8_t kAddrs[] = {0x1A, 0x10, 0};

static bool detect(uint8_t *addr7_out) { return imx708_detect(addr7_out); }

static bool configure(uint8_t addr7, esp32p4_cam_framesize_t want, esp32p4_cam_mode_t *mode_out) {
  esp32p4_cam_mode_t m{};
  m.bayer = ESP32P4_BAYER_RGGB;
  m.in_fmt = ESP32P4_CAM_IN_RAW10;
  m.lanes = 2;
  bool ok = false;
  if (want == ESP32P4_FRAMESIZE_2304X1296) {
    m.name = "IMX708 2304x1296 RAW10";
    m.width = 2304;
    m.height = 1296;
    m.lane_mbps = 800;
    m.framesize_tag = ESP32P4_FRAMESIZE_2304X1296;
    ok = imx708_configure_2304x1296(addr7);
  } else {
    m.name = "IMX708 1280x720 RAW10";
    m.width = 1280;
    m.height = 720;
    m.lane_mbps = 400;
    m.framesize_tag = ESP32P4_FRAMESIZE_HD;
    ok = imx708_configure_hd720(addr7);
  }
  if (ok && mode_out) *mode_out = m;
  return ok;
}

static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX708,
    "IMX708",
    ESP32P4_CAM_SUPPORT_FULL,
    kAddrs,
    detect,
    configure,
    imx708_stream_on,
    imx708_stream_off,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

const esp32p4_cam_sensor_ops_t *imx708_sensor_ops(void) { return &kOps; }
