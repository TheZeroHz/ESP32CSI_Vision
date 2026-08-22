# ESP32CSI_Vision

Arduino / ESP-IDF library for **ESP32-P4** cameras. Default path is MIPI CSI → `camera_fb_t` (RGB565). DVP, SPI, and USB-host UVC share the same framebuffer type.

**Author:** [Rakib Hasan](https://github.com/thezerohz) (@thezerohz) · **Version:** 3.25.0 · **License:** MIT

| You want | Open |
| --- | --- |
| Install + first sketch | [docs/wiki/Getting-Started.md](docs/wiki/Getting-Started.md) |
| Pins for *your* carrier | [docs/Custom-Boards.md](docs/Custom-Boards.md) |
| Class / method catalog | [docs/wiki/API-Reference.md](docs/wiki/API-Reference.md) |
| Enums and structs | [docs/wiki/Enums-and-Types.md](docs/wiki/Enums-and-Types.md) |
| MJPEG HTTP + `/control` | [docs/wiki/HTTP-and-Preferences.md](docs/wiki/HTTP-and-Preferences.md) |
| Every example | [docs/wiki/Examples-Map.md](docs/wiki/Examples-Map.md) |
| CSI sensor matrix | [docs/CSI-Cameras.md](docs/CSI-Cameras.md) |
| ESP-DL weights | [models/espdl/README.md](models/espdl/README.md) |

---

## Requirements

- Chip: **ESP32-P4** + **PSRAM enabled**
- Arduino-ESP32 **3.3.x** (or ESP-IDF 5.3–5.5 for `idf_examples/`)
- Board: any P4 CSI carrier. Pins live in **`board_config.h` next to the sketch**, not in `src/`.

Typical Arduino board menu: ESP32P4 Dev Module · PSRAM Enabled · 16 MB flash · partition `app3M_fat9M_16MB`.

```text
esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
```

Install: clone into `Documents/Arduino/libraries/ESP32CSI_Vision`, or Sketch → Include Library → Add .ZIP Library.

---

## Programming model

1. **Include the sketch config**, not a hidden Guition preset:

```cpp
#include "board_config.h"   // Arduino tab next to the .ino; includes <ESP32CSI_Vision.h>
```

2. **Use objects.** Sketches declare `ESP32P4_Camera cam;` then `cam.begin(...)`, `det.infer(fb)`, `dbg.poll()`. Do not call `Class::method` from examples.

3. **Own the framebuffer.** `capture()` returns a pool slot. Always `cam.release(fb)`.

4. **Models** (optional) live at `/models/p4/*.espdl` on SD or flash. Copy from [`models/espdl/p4/`](models/espdl/p4/).

---

## Quick start

### 1. Capture

```cpp
#include "board_config.h"

ESP32P4_Camera cam;
ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  dbg.begin("01_CamTest", ESP32P4_DBG_CAM);

  esp32p4_cam_config_t cfg = esp32csi_cam_config();  // GPIOs from board_config.h
  if (!cam.begin(cfg)) while (true) delay(1000);
}

void loop() {
  dbg.poll();
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) return;
  // default: RGB565, fb->len = width * height * 2
  cam.release(fb);
}
```

Flash **`00_BoardConfig`** first and read Serial `CFG:` lines. Then **`01_CamTest`**.

### 2. MJPEG UI (C6 Wi-Fi)

```cpp
#include "board_config.h"

ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;

void setup() {
  cam.begin(esp32csi_cam_config());
  esp32csi_wifi_begin();                 // CFG_WIFI_* in board_config.h
  stream.begin(&cam, 80, 35);            // UI :80, MJPEG :81
}

void loop() {
  stream.loop();
}
```

Example: `04_WiFiMjpeg`. Ethernet equivalent: `30_EthLiveAvFiles`.

### 3. You own the FB + a model

```cpp
camera_fb_t *fb = cam.capture();
int n = det.infer(fb);
det.draw((uint16_t *)fb->buf, fb->width, fb->height, det.results(), n, det.model());
preview.setStatusJson(json);
preview.present(fb);
cam.release(fb);
```

Example: **`43_CamWebModels`**. Serial-only JSON: **`42_DetectApi`**.

```cpp
ESP32P4_ObjectDetect det;
det.begin(ESP32P4_DET_DOG_224);   // also ESP32P4_DET_COCO_YOLO11N, CAT_224, …
```

---

## Module catalog

Umbrella: `#include <ESP32CSI_Vision.h>` (pulled in by `board_config.h`). Full methods: [API Reference](docs/wiki/API-Reference.md).

| Area | Object | Header |
| --- | --- | --- |
| Capture | `ESP32P4_Camera` | `cam/ESP32P4_Camera.h` |
| JPEG | `ESP32P4_Jpeg` | `jpeg/ESP32P4_Jpeg.h` |
| H.264 / MP4 | `ESP32P4_H264` | `h264/ESP32P4_H264.h` |
| PPA scale | `ESP32P4_Ppa` | `ppa/ESP32P4_Ppa.h` |
| MJPEG UI | `ESP32P4_MjpegServer` | `stream/ESP32P4_Mjpeg.h` |
| Sketch-fed preview | `ESP32P4_WebPreview` | `stream/ESP32P4_WebPreview.h` |
| USB gadget webcam | `ESP32P4_Uvc` | `uvc/ESP32P4_Uvc.h` |
| USB-host UVC | `cam` + `ESP32P4_CAM_BUS_UVC_HOST` | same Camera |
| V4L2 POSIX | `ESP32P4_V4l2` | `v4l2/ESP32P4_V4l2.h` |
| SDMMC | `ESP32P4_Sd` | `sd/ESP32P4_Sd.h` |
| SD / FFat / LittleFS | `ESP32P4_StoragePref` | `storage/ESP32P4_StoragePref.h` |
| ES8311 mic | `ESP32P4_Mic` | `audio/ESP32P4_Mic.h` |
| Imgproc | `ESP32P4_Cv` | `cv/ESP32P4_Cv.h` |
| `Mat` / ROI | `esp_cv::Mat` | `opencv/esp_cv.hpp` |
| Face | `ESP32P4_FaceAi` | `face/ESP32P4_FaceAi.h` |
| Boxes | `ESP32P4_ObjectDetect` | `detect/ESP32P4_ObjectDetect.h` |
| Pose / seg / cls / … | `ESP32P4_Pose` `Seg` `Cls` `Gesture` `Reid` `Ocr` | `detect/` |
| QR / barcode | `ESP32P4_Qr` | `qr/ESP32P4_Qr.h` |
| File manager | `WebFileManager` | `wfm/WebFileManager.h` |
| Debug | `ESP32P4_Debug` | `debug/ESP32P4_Debug.h` |

CSI sensors (OV5647, IMX477, SC2336, …): [CSI-Cameras.md](docs/CSI-Cameras.md). One MIPI CSI host on P4; CSI + DVP (or SPI / host UVC) can run as two objects (`28_DualCam`).

---

## Examples (Arduino IDE)

**File → Examples → ESP32CSI_Vision → …**  
Each folder has its own `board_config.h`. Map: [Examples-Map.md](docs/wiki/Examples-Map.md).

| Start with | Sketch |
| --- | --- |
| Print wiring | `00_BoardConfig` |
| Capture | `01_CamTest` |
| Wi-Fi MJPEG | `04_WiFiMjpeg` |
| SD | `09_SdCard` |
| Mic WAV | `15_MicSdRecord` |
| Video + mic MP4 (no stream) | `44_AvSdRecord` |
| DVP / SPI / host UVC | `24` / `25` / `26` |
| Ethernet live AV | `30_EthLiveAvFiles` |
| Detect JSON | `42_DetectApi` |
| Detect + web preview | `43_CamWebModels` |

ESP-IDF: `idf_examples/08_FaceDetect`, `09_CocoDetect`, `21_EthFaceWeb`.

---

## Layout

```text
src/ESP32CSI_Vision.h     umbrella (no board pins)
src/cam jpeg ppa dsp      capture / encode / scale / motion
src/stream                MJPEG UI + WebPreview
src/detect face espdl     ESP-DL zoo
src/h264 audio sd         record / mic / SDMMC
examples/*/board_config.h YOUR GPIOs (per sketch)
docs/wiki                 API + HTTP + examples
```

**Rules of thumb:** `cam.release(fb)` after every `capture()`. Keep MJPEG on `:81` so `:80` `/control` stays live. Prefer microSD for long H.264. Face / detect `.espdl` files must be on a mounted volume before `begin()`.

Changelog: [CHANGELOG.md](CHANGELOG.md).
