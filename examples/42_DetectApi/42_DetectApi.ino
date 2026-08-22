/**
 * 42_DetectApi — image in, boxes + JSON out (no web UI)
 *
 * Pass camera_fb_t (or RGB565 / RGB888 / JPEG) into a model.
 * For live preview of the same FB, use 43_CamWebModels.
 *
 * Board: board_config.h
 */

#ifndef APP_STORAGE
#define APP_STORAGE ESP32P4_STORAGE_AUTO
#endif
#ifndef APP_NAME
#define APP_NAME "42_DetectApi"
#endif
#ifndef APP_MODEL
#define APP_MODEL ESP32P4_DET_DOG_224
#endif
#ifndef APP_SCORE
#define APP_SCORE 0.35f
#endif

#include "board_config.h"

ESP32P4_Camera cam;
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_ObjectDetect det;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 42_DetectApi ===");

  if (!store.begin(APP_STORAGE, false, &sd, (esp32p4_board_t)ESP32CSI_BOARD)) {
    Serial.println("No volume yet — camera still runs; models need /models/p4/*.espdl");
  } else {
    Serial.printf("Storage volumes: %s (primary %s)\n", store.volumeSummary(), store.label());
  }

  esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
  esp32csi_print_cam_config(cam_cfg);
  if (!cam.begin(cam_cfg)) {
    Serial.println("camera FAILED");
    while (true) delay(1000);
  }

  auto model = (int)APP_MODEL;
  Serial.printf("Loading %s (%s)\n", det.modelName(model),
                det.modelFile(model));
  if (!det.begin(model)) {
    Serial.println("det.begin FAILED — copy the .espdl to /models/p4/ on any mounted volume");
  } else {
    det.setScoreThr(APP_SCORE);
    Serial.println("detect ready — Serial prints boxes + JSON");
  }
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) {
    Serial.println("capture timeout");
    delay(200);
    return;
  }

  int n = det.infer(fb);
  const esp32p4_det_t *dets = det.results();
  Serial.printf("n=%d ms=%d\n", n, det.lastMs());
  for (int i = 0; i < n; i++) {
    const esp32p4_det_t &d = dets[i];
    Serial.printf("  %s  class=%d  score=%.2f  box x=%d y=%d w=%d h=%d\n", d.label ? d.label : "?",
                  d.category, (double)d.score, d.x, d.y, d.w, d.h);
  }

  char json[768];
  det.resultsJson(json, sizeof(json));
  Serial.println(json);

  cam.release(fb);
  delay(200);
}
