# ESP32CSI_Vision wiki

**Library:** [ESP32CSI_Vision](https://github.com/thezerohz/ESP32CSI_Vision) · **Author:** [Rakib Hasan](https://github.com/thezerohz) (@thezerohz)  
**Version:** 3.25.0 · **Target:** ESP32-P4 + PSRAM · **Arduino-ESP32 3.3.x** (ESP-IDF 5.3–5.5 for `idf_examples/`)

Programmer docs for capture, encode, stream, storage, and the ESP-DL model zoo.

---

## Pages

| Page | Use it for |
| --- | --- |
| [Getting Started](Getting-Started.md) | Install, board menu, first `capture()` |
| [Custom boards](../Custom-Boards.md) | Camera / SD / mic / Wi-Fi / ETH pins in `board_config.h` |
| [API Reference](API-Reference.md) | Every public class, grouped by domain |
| [Enums & Types](Enums-and-Types.md) | Framesizes, pixformats, model IDs, structs |
| [HTTP & Preferences](HTTP-and-Preferences.md) | MJPEG `:80` / `:81`, `/control`, `/debug` |
| [Examples Map](Examples-Map.md) | What each sketch teaches |
| [CSI cameras](../CSI-Cameras.md) | Sensor matrix, IPA, MIPI notes |
| [ESP-DL models](../../models/espdl/README.md) | `.espdl` files under `/models/p4/` |

Landing page: [README](../../README.md) · history: [CHANGELOG](../../CHANGELOG.md)

---

## How a sketch is structured

```text
sketch.ino
  #include "board_config.h"     // YOUR GPIOs; includes <ESP32CSI_Vision.h>
  ESP32P4_Camera cam;
  ESP32P4_Debug  dbg;
  cam.begin(esp32csi_cam_config());
  fb = cam.capture();  …  cam.release(fb);
```

| Rule | Detail |
| --- | --- |
| Objects | `cam.begin()`, `det.infer(fb)`, `dbg.poll()` — not `Class::method` in sketches |
| Framebuffer | Every `capture()` needs a matching `release(fb)` |
| Pins | Edit the example’s `board_config.h`, not `src/` |
| Models | `{/sdcard|/ffat|/littlefs|/spiffs}/models/p4/*.espdl` |

---

## Module tree

```text
#include <ESP32CSI_Vision.h>          // also pulled in by board_config.h
        │
        ├── Capture
        │     ESP32P4_Camera          CSI / DVP / SPI / USB-host UVC → camera_fb_t
        │     ESP32P4_V4l2            optional /dev/video* + v4l2-ctl
        │     ESP32P4_Uvc             USB gadget webcam (JPEG) — not host UVC
        │
        ├── Encode / scale
        │     ESP32P4_Jpeg            HW encode / decode
        │     ESP32P4_H264            HW Baseline + MP4
        │     ESP32P4_Ppa             SRM scale / rotate / mirror
        │     ESP32P4_Img             RGB565 helpers
        │
        ├── Stream
        │     ESP32P4_MjpegServer     fat Camera UI (worker captures for you)
        │     ESP32P4_WebPreview      you capture() → present(fb)
        │
        ├── Storage / audio
        │     ESP32P4_Sd              SDMMC
        │     ESP32P4_StoragePref     SD + FFat + LittleFS + SPIFFS
        │     ESP32P4_Mic             ES8311
        │     WebFileManager          HTTP file browser
        │
        ├── CV / motion
        │     ESP32P4_Cv              gray / HSV / blobs
        │     ESP32P4_Dsp             frame-diff motion
        │     ESP32P4_Qr              QR / barcode
        │     ESP32P4_VisionAi        letterbox / NMS / result structs
        │     ESP32P4_WhoPipeline     async capture queue
        │
        └── ESP-DL (Arduino + IDF)
              ESP32P4_ObjectDetect    COCO / YOLO26 / cat / dog / hand / pedestrian
              ESP32P4_Pose Seg Cls Gesture Reid Ocr Speaker
              ESP32P4_FaceDetect      boxes + 5 landmarks
              ESP32P4_FaceAi          detect + enroll / recognize
              ESP32P4_Debug           Serial + NVS + /debug
```

One MIPI CSI host on P4. CSI + DVP (or SPI / host UVC) can run as two `ESP32P4_Camera` objects (`28_DualCam`). USB gadget (`23_UsbUvc`) and USB-host UVC (`26`) cannot share the USB PHY.

---

## Conventions

| Symbol | Meaning |
| --- | --- |
| `bool` | `true` = success |
| `camera_fb_t *` | Pool slot — always `cam.release(fb)` |
| `int` from `infer` / `detect` | Hit count (`0` = none, `<0` = error) |
| PSRAM | Required for multi-FB + JPEG + models |
| `ESP32P4_DET_*` / `ESP32P4_FACE_*` | Global model IDs for sketches |
