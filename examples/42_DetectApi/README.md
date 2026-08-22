# 42_DetectApi

Minimal sketch showing how to feed an image into an ESP-DL detector and read structured results — no MJPEG UI, no Ethernet, **no SD card required**.

## Merge into another project

```cpp
#include <ESP32CSI_Vision.h>

ESP32P4_ObjectDetect det;
esp32p4_det_t boxes[16];

void setup() {
  det.begin(ESP32P4_DET_DOG_224);  // or ESP32P4_DET_CAT_224, ESP32P4_DET_COCO_YOLO11N, …
}

void loop() {
  camera_fb_t *fb = cam.capture();
  int n = det.detect(fb, boxes, 16);         // RGB565 framebuffer
  // int n = det.infer(fb);                  // same, stores in det.results()
  // int n = det.detectRgb888(rgb, w, h, boxes, 16);
  // int n = det.detectJpeg(jpeg, jpg, len, boxes, 16);

  for (int i = 0; i < n; i++) {
    // boxes[i].label   "dog" / "cat" / "person" / …
    // boxes[i].score   0..1
    // boxes[i].category  class id
    // boxes[i].x, y, w, h  pixels in the source image
  }

  char json[512];
  det.toJson(boxes, n, json, sizeof(json), det.lastMs());
  // {"n":1,"ms":42,"dets":[{"label":"dog","class":0,"score":0.91,"x":10,"y":20,"w":100,"h":80}]}
}
```

Storage is **SD-optional**. Put weights at `/models/p4/*.espdl`.

Board pins: `board_config.h` in this folder. Live preview of the same FB: example **43_CamWebModels**.
