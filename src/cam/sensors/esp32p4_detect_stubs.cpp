#include "cam/esp32p4_cam_sensor_ops.h"
#include "cam/esp32p4_sccb.h"

static bool stub_configure(uint8_t, esp32p4_cam_framesize_t, esp32p4_cam_mode_t *) { return false; }
static bool stub_stream_on(uint8_t) { return false; }
static bool stub_stream_off(uint8_t) { return true; }

namespace ov7251_stub {
static const uint8_t kAddrs[] = {0x60, 0x10, 0};
static bool detect(uint8_t *o) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t id = 0;
    if (esp32p4_sccb_read16(*p, 0x300A, &id) && id == 0x7750) {
      if (o) *o = *p;
      return true;
    }
  }
  return false;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_OV7251, "OV7251", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, stub_configure,
    stub_stream_on, stub_stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace

namespace imx296_stub {
static const uint8_t kAddrs[] = {0x1A, 0x10, 0};
static bool detect(uint8_t *o) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t id = 0;
    if (esp32p4_sccb_read16(*p, 0x3F12, &id) && (id == 0x0296 || id == 0x296)) {
      if (o) *o = *p;
      return true;
    }
  }
  return false;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX296, "IMX296", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, stub_configure,
    stub_stream_on, stub_stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace

namespace imx462_stub {
static const uint8_t kAddrs[] = {0x1A, 0x10, 0};
static bool detect(uint8_t *o) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    uint16_t id = 0;
    if (esp32p4_sccb_read16(*p, 0x3F12, &id) && id == 0x0462) {
      if (o) *o = *p;
      return true;
    }
  }
  return false;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_IMX462, "IMX462", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect, stub_configure,
    stub_stream_on, stub_stream_off, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace

namespace arducam_imx500_stub {
static const uint8_t kAddrs[] = {0x0C, 0x1A, 0};
static bool detect(uint8_t *o) {
  for (const uint8_t *p = kAddrs; *p; ++p) {
    if (!esp32p4_sccb_ping(*p)) continue;
    // Arducam modules vary; presence at 0x0C is a soft signal.
    if (*p == 0x0C) {
      if (o) *o = *p;
      return true;
    }
  }
  return false;
}
static const esp32p4_cam_sensor_ops_t kOps = {
    ESP32P4_SENSOR_ARDUCAM_IMX500, "Arducam-IMX500", ESP32P4_CAM_SUPPORT_DETECT_ONLY, kAddrs, detect,
    stub_configure, stub_stream_on, stub_stream_off, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
}  // namespace

const esp32p4_cam_sensor_ops_t *ov7251_sensor_ops(void) { return &ov7251_stub::kOps; }
const esp32p4_cam_sensor_ops_t *imx296_sensor_ops(void) { return &imx296_stub::kOps; }
const esp32p4_cam_sensor_ops_t *imx462_sensor_ops(void) { return &imx462_stub::kOps; }
const esp32p4_cam_sensor_ops_t *arducam_imx500_sensor_ops(void) {
  return &arducam_imx500_stub::kOps;
}
