/**
 * 28_DualCam — CSI + a second bus (DVP / SPI / UVC-HOST).
 *
 * ESP32-P4 has one MIPI CSI host — two CSI modules will not work.
 * CSI pins: board_config.h CFG_CAM_*. Second bus: CFG_DVP_* / CFG_SPI_* / CFG_UVC_*.
 * Set APP_SECOND_BUS below (default DVP).
 */

#include "board_config.h"

#ifndef APP_NAME
#define APP_NAME "28_DualCam"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif

#ifndef APP_SECOND_BUS
#define APP_SECOND_BUS ESP32P4_CAM_BUS_DVP
#endif

ESP32P4_Camera csi;
ESP32P4_Camera cam1;

static bool begin_second() {
  if (!esp32p4_cam_dual_ok(ESP32P4_CAM_BUS_CSI, APP_SECOND_BUS)) {
    Serial.printf("dual: %s\n", esp32p4_cam_dual_why(ESP32P4_CAM_BUS_CSI, APP_SECOND_BUS));
    return false;
  }
  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  cfg.bus = APP_SECOND_BUS;
  cfg.fb_count = 2;
  if (cfg.bus == ESP32P4_CAM_BUS_DVP) {
    cfg.sensor = ESP32P4_SENSOR_OV2640;
    cfg.frame_size = ESP32P4_FRAMESIZE_VGA;
    cfg.pixel_format = ESP32P4_PIXFORMAT_RGB565;
    cfg.xclk_hz = 20000000;
    cfg.sda = CFG_DVP_SDA;
    cfg.scl = CFG_DVP_SCL;
  } else if (cfg.bus == ESP32P4_CAM_BUS_SPI) {
    cfg.sensor = ESP32P4_SENSOR_SP0A39;
    cfg.frame_size = ESP32P4_FRAMESIZE_VGA;
    cfg.pixel_format = ESP32P4_PIXFORMAT_GRAY8;
  } else if (cfg.bus == ESP32P4_CAM_BUS_UVC_HOST) {
    cfg.pixel_format = ESP32P4_PIXFORMAT_JPEG;
    cfg.uvc.width = 640;
    cfg.uvc.height = 480;
    cfg.uvc.format = 1;
  }
  return cam1.begin(cfg);
}

ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 28_DualCam ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  Serial.println("P4: one CSI host. Second camera is DVP / SPI / UVC-HOST.");

  if (!csi.begin(esp32csi_cam_config())) {
    Serial.println("CSI begin FAILED — CFG_CAM_* in board_config.h");
    while (true) delay(1000);
  }
  Serial.printf("cam0 %s %s  %ux%u\n", csi.busName(), csi.sensorName(), csi.width(), csi.height());

  if (!begin_second()) {
    Serial.println("cam1 begin FAILED — CSI still runs. Check second-bus pins in board_config.h.");
  } else {
    Serial.printf("cam1 %s %s  %ux%u\n", cam1.busName(), cam1.sensorName(), cam1.width(),
                  cam1.height());
  }
}

void loop() {
  dbg.poll();
  camera_fb_t *a = csi.capture(2000);
  camera_fb_t *b = cam1.width() ? cam1.capture(2000) : nullptr;
  if (a) {
    Serial.printf("csi  %ux%u  %u bytes  %s\n", a->width, a->height, (unsigned)a->len,
                  csi.formatName());
    csi.release(a);
  } else {
    Serial.println("csi capture timeout");
  }
  if (b) {
    Serial.printf("cam1 %ux%u  %u bytes  %s\n", b->width, b->height, (unsigned)b->len,
                  cam1.formatName());
    cam1.release(b);
  }
  delay(400);
}
