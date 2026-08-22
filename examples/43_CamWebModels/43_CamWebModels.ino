/**
 * 43_CamWebModels — you own the camera FB
 *
 *   cam.capture() → det.infer(fb) → draw → preview.present(fb) → release
 *
 * Camera + Wi-Fi: board_config.h (docs/Custom-Boards.md).
 * Model: /models/p4/<file>.espdl on SD or flash FS.
 */

#ifndef APP_NAME
#define APP_NAME "43_CamWebModels"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_CAM
#endif
#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif

#include "board_config.h"

#ifndef APP_HTTP_PORT
#define APP_HTTP_PORT 80
#endif
#ifndef APP_JPEG_QUALITY
#define APP_JPEG_QUALITY 40
#endif
#ifndef APP_SCORE
#define APP_SCORE 0.35f
#endif

/* Pick one detect model. Others: COCO_YOLO11N, COCO_YOLO11N_320, YOLO26_640,
 * PEDESTRIAN_PICO, CAT_224, CAT_416, DOG_416, HAND_224 */
#ifndef APP_MODEL
#define APP_MODEL ESP32P4_DET_DOG_224
#endif

ESP32P4_Camera cam;
ESP32P4_WebPreview preview;
ESP32P4_ObjectDetect det;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_Debug dbg;

static char g_json[1024];

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 43_CamWebModels ===");
  dbg.begin(APP_NAME, APP_DEBUG);
  esp32csi_print_cam_config(esp32csi_cam_config());
  esp32csi_print_wifi_config(esp32csi_wifi_config());

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("APP: no storage yet — camera still runs; copy .espdl when a volume is mounted");
  } else {
    Serial.printf("APP: storage %s  (%s)\n", store.label(), store.volumeSummary());
  }

  esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
  /* Sketch still owns every field — change anything here: */
  cam_cfg.pixel_format = ESP32P4_PIXFORMAT_RGB565;
  /* cam_cfg.sensor = ESP32P4_SENSOR_IMX477; */
  /* cam_cfg.frame_size = ESP32P4_FRAMESIZE_1080P; */
  /* cam_cfg.wire = &Wire1; */
  esp32csi_print_cam_config(cam_cfg);
  if (!cam.begin(cam_cfg)) {
    Serial.println("camera begin FAILED — check CSI ribbon, LDO, SDA/SCL, sensor");
    while (true) delay(1000);
  }
  Serial.printf("APP: live %s  %ux%u  %s\n", cam.sensorName(), cam.width(), cam.height(),
                cam.formatName());

  auto model = (int)APP_MODEL;
  Serial.printf("APP: loading %s  file=%s  score>=%.2f\n", det.modelName(model),
                det.modelFile(model), (double)APP_SCORE);
  if (!det.begin(model)) {
    Serial.println("APP: det.begin FAILED — put the .espdl in /models/p4/ on any volume");
  } else {
    det.setScoreThr(APP_SCORE);
  }

  if (!esp32csi_wifi_begin()) {
    Serial.println("Wi-Fi FAILED");
    while (true) delay(1000);
  }
  if (!preview.begin(APP_HTTP_PORT, APP_JPEG_QUALITY, cam.width(), cam.height())) {
    Serial.println("WebPreview begin FAILED");
    while (true) delay(1000);
  }
  preview.setTitle(APP_NAME);

  IPAddress ip = esp32csi_wifi_ip();
  Serial.printf("APP: open  http://%s/\n", ip.toString().c_str());
  Serial.printf("APP: json  http://%s/dets\n", ip.toString().c_str());
  Serial.println("APP: loop = capture → infer → draw → present(fb) → release");
}

void loop() {
  dbg.poll();
  preview.loop();

  camera_fb_t *fb = cam.capture(2000);
  if (!fb) {
    Serial.println("capture timeout");
    delay(50);
    return;
  }

  int n = 0;
  if (det.ready()) {
    n = det.infer(fb);
    if (fb->format == ESP32P4_PIXFORMAT_RGB565) {
      det.draw((uint16_t *)fb->buf, fb->width, fb->height, det.results(), n,
                                 det.model());
    }
    det.resultsJson(g_json, sizeof(g_json));
  } else {
    snprintf(g_json, sizeof(g_json),
             "{\"n\":0,\"error\":\"model missing\",\"want\":\"/models/p4/%s\"}",
             det.modelFile((int)APP_MODEL));
  }
  preview.setStatusJson(g_json);

  if (!preview.present(fb)) Serial.println("present(fb) FAILED");
  cam.release(fb);

  static uint32_t last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    Serial.printf("n=%d ms=%d jpeg=%u  %s\n", n, det.lastMs(), (unsigned)preview.lastJpegBytes(),
                  g_json);
  }
}
