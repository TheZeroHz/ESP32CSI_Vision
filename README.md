# ESP32CSI_Vision

Arduino library for **ESP32-P4 MIPI CSI** cameras: capture, JPEG/H.264, MJPEG web UI, SD, OpenCV-like CV, and ESP-DL face detect/recognize.

```cpp
#include <ESP32CSI_Vision.h>
```

**Target:** ESP32-P4 + PSRAM · **Arduino-ESP32 3.3.x** · Tested board: Guition JC-ESP32P4-M3 (OV5647 / IMX708)

---

## Install

Copy or clone into:

```text
Documents/Arduino/libraries/ESP32CSI_Vision
```

Board settings (typical):

| Setting | Value |
| --- | --- |
| Board | ESP32P4 Dev Module |
| PSRAM | **Enabled** |
| Flash Size | 16MB (if applicable) |
| Partition | `app3M_fat9M_16MB` (or similar large app) |

FQBN example:

```text
esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
```

---

## Modules (what you get)

| Module | Class / API | Purpose |
| --- | --- | --- |
| Camera | `ESP32P4_Camera` | MIPI CSI → RGB565 in PSRAM |
| JPEG | `ESP32P4_Jpeg` | HW encode / decode |
| PPA | `ESP32P4_Ppa` | HW scale / rotate90 / mirror / RGB565→gray |
| Stream | `ESP32P4_MjpegServer` | Web UI `:80` + MJPEG `:81` |
| SD | `ESP32P4_Sd` | microSD mount / files |
| Capture | `enableSdCapture()` | JPEG stills → `/IMG` |
| Record | `enableVideoRecord()` | H.264 MP4 → `/VIDEO` |
| Mic | `ESP32P4_Mic` | ES8311 levels + PCM into MP4 |
| CV | `ESP32P4_Cv` | Gray, blur, thresh, morph, HSV blobs, edges, draw |
| OpenCV core | `esp_cv::Mat` / `cv::Mat` | Mat, Point, Rect, Scalar, ROI, PSRAM |
| Face | `ESP32P4_FaceAi` | Detect + enroll + recognize (`.espdl` on SD) |
| Files | `WebFileManager` | Browser for SD (usually `:82`) |
| DSP | `ESP32P4_Dsp` | Motion / frame difference |
| Mem | `esp32p4_psram_*` | Aligned PSRAM alloc |

---

## Quick start

### Capture a frame

```cpp
#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;

void setup() {
  Serial.begin(115200);
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    while (true) delay(1000);
  }
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);  // timeout ms
  if (!fb) return;
  // fb->buf = RGB565, fb->len = w*h*2
  cam.release(fb);                      // always release
}
```

### MJPEG web UI

```cpp
ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;

void setup() {
  cam.begin(ESP32P4_BOARD_GUITION_M3);
  // WiFi / Ethernet bring-up here…
  stream.begin(&cam, 80, 35);   // UI :80, stream :81, JPEG quality 35
}

void loop() {
  stream.loop();
}
```

Open `http://<ip>/` (settings) and `http://<ip>:81/stream` (MJPEG).

### Stills + video record

```cpp
ESP32P4_Sd sd;
ESP32P4_H264 h264;
ESP32P4_Mic mic;

sd.begin(ESP32P4_BOARD_GUITION_M3);
h264.begin(640, 480, 30, 1500000);
mic.begin(16000);

stream.begin(&cam, 80, 35);
stream.enableSdCapture(&sd, "/IMG");           // Capture → SD
stream.enableVideoRecord(&sd, &h264, "/VIDEO"); // Record / Stop
if (mic.ready()) stream.enableMic(&mic);
```

### Face detect / enroll (Arduino)

Models on SD (FAT):

```text
/models/p4/human_face_detect_msr_s8_v1.espdl
/models/p4/human_face_detect_mnp_s8_v1.espdl
/models/p4/human_face_feat_mfn_s8_v1.espdl      // enroll / recognize
```

```cpp
ESP32P4_FaceAi face;
face.begin(ESP32P4_FaceDetect::MSRMNP_S8_V1, "/sdcard/face/face.db", "/sdcard/face/names.txt");

esp32p4_face_id_t out[8];
int n = face.run(rgb565, w, h, out, 8, /*recognize=*/true);
ESP32P4_FaceAi::draw(rgb565, w, h, out, n);

face.requestEnroll("Alice");   // N samples → one feature
face.cancelEnroll();
face.deleteName("Alice");
face.setThresh(0.5f);
```

See example `21_EthFaceWeb`.

---

## API reference

### `ESP32P4_Camera`

| Call | Meaning |
| --- | --- |
| `begin(board)` / `begin(cfg)` | Init CSI + ISP + PSRAM framebuffers |
| `end()` | Shutdown |
| `capture(timeout_ms)` | Next RGB565 frame, or `nullptr` |
| `release(fb)` | Return FB to pool |
| `width()` / `height()` | Active size |
| `sensorName()` / `detected()` | Sensor info |
| `setHMirror` / `setVFlip` | Geometry (+ ISP Bayer sync) |
| `setAEC` / `setAGC` | Auto exposure / gain |
| `setExposure(lines)` / `setGain` / `setGainCeiling` | Manual exposure path |
| `get*()` counterparts | Read back |
| `setTestPattern(bool)` | Sensor color bars |

**Boards:** `ESP32P4_BOARD_GUITION_M3`, `WAVESHARE_NANO`, `FUNCTION_EV`, or custom `esp32p4_cam_config_t`.

**`camera_fb_t`:** `buf`, `len`, `width`, `height`, `format`, `timestamp_us`.

### `ESP32P4_Jpeg`

| Call | Meaning |
| --- | --- |
| `begin(max_w, max_h, quality)` | Init HW JPEG (quality ~4–63 typical for stream) |
| `encode(fb\|rgb, …)` | RGB565 → JPEG bytes |
| `decode(jpg, …)` | JPEG → RGB |
| `setQuality(q)` | Change quality |

### `ESP32P4_Ppa`

| Call | Meaning |
| --- | --- |
| `begin()` | Init PPA |
| `scale` / `scaleFit` / `scaleCover` | HW resize RGB565 |
| `rotate90` / `mirror` | Geometry |
| `rgb565ToGray` / `rgb565ToGrayScale` | HW color convert (+ optional scale) |

### `ESP32P4_MjpegServer`

| Call | Meaning |
| --- | --- |
| `begin(cam, port, quality)` | HTTP UI on `port`, MJPEG on `port+1` |
| `loop()` | Call from Arduino `loop()` |
| `setQuality` / `setFrameSkip` / `setFramesize` | Live stream knobs |
| `enableSdCapture(sd, folder)` | UI “Capture → SD” |
| `enableVideoRecord(sd, h264, folder)` | UI Record / Stop → MP4 |
| `enableMic(mic)` | Waveform + PCM in MP4 |
| `setFrameHook(fn, user)` | Annotate RGB565 before JPEG (not the H.264 FB) |
| `enableCvDashboard(true)` | Built-in CV modes in UI |
| `enableFaceUi(true)` / `faceUi()` | Face panel state for sketch-driven FR |
| `setFilesBrowserPort(port)` | Link to WebFileManager |

**Stream framesize (`setFramesize` / `/control?var=framesize`):**

| Value | Enum | Size |
| :---: | --- | --- |
| 0 | `ESP32P4_STREAM_FHD` | 1920×1072 |
| 1 | `ESP32P4_STREAM_HD` | 1280×720 |
| 2 | `ESP32P4_STREAM_XGA` | 1024×576 |
| 3 | `ESP32P4_STREAM_SVGA` | 800×640 |
| 4 | `ESP32P4_STREAM_VGA` | 640×480 |
| 5 | `ESP32P4_STREAM_HVGA` | 480×320 |
| 6 | `ESP32P4_STREAM_CIF` | 400×288 |
| 7 | `ESP32P4_STREAM_QVGA` | 320×240 |
| 8 | `ESP32P4_STREAM_HQVGA` | 240×176 |
| 9 | `ESP32P4_STREAM_QQVGA` | 160×128 |

Face detect/enroll locks stream output to the model size (e.g. 320×240) while active.

### HTTP (camera UI)

| Endpoint | Port | Role |
| --- | :---: | --- |
| `GET /` | 80 | Settings UI |
| `GET /control?var=&val=` | 80 | Change setting |
| `GET /status` | 80 | JSON status |
| `GET /capture` / `/jpg` | 80 | One JPEG in browser |
| `GET /capture_img` | 80 | Save JPEG to SD (`/IMG`) |
| `GET /stream` | 81 | Multipart MJPEG |

**Common `var` names:**

| `var` | `val` | Meaning |
| --- | --- | --- |
| `quality` | 4–63 | JPEG quality |
| `framesize` | 0–9 | Stream size (table above) |
| `frameskip` | 0–8 | Skip N frames between encodes |
| `hmirror` / `vflip` | 0/1 | Mirror / flip |
| `aec` / `agc` | 0/1 | Auto exposure / gain |
| `aec_value` | 4–980 | Manual exposure lines (turns AEC off in UI) |
| `agc_gain` | 0–1023 | Manual gain |
| `gainceiling` | 16–1023 | Gain ceiling |
| `colorbar` | 0/1 | Test pattern |
| `face_detect` / `face_recog` | 0/1 | Face panel |
| `face_thr` | 10–95 | Match threshold % |
| `face_enroll` | name | Start enroll |
| `face_model` | 0/1/2 | MSR+MNP / ESPDet224 / ESPDet416 |

### `ESP32P4_Cv` (imgproc on RGB565 / GRAY8)

| Call | Meaning |
| --- | --- |
| `toGray` / `toGrayScale` | RGB565 → gray (PPA when possible) |
| `blur3x3` | Box blur |
| `threshold` / `otsu` / `adaptiveThreshold` | Binarize |
| `erode` / `dilate` / `morphologyOpen` / `Close` | Morphology |
| `rgb565ToHsv` / `inRangeHsv` | HSV mask |
| `edges` | Sobel magnitude + dual threshold |
| `findBlobs` | Connected components → boxes |
| `line` / `circle` / `putText` / `drawBlob` | Draw on RGB565 |

### `esp_cv` / `cv` (OpenCV-like core)

```cpp
#include <ESP32CSI_Vision.h>

esp_cv::Mat m(240, 320, esp_cv::CV_8UC1);          // PSRAM-owned
esp_cv::Mat view = m(esp_cv::Rect(10, 10, 64, 64)); // ROI (no copy)
esp_cv::Mat wrap = esp_cv::wrapRgb565(buf, w, h);   // external FB

// Optional aliases: cv::Mat, cv::Point, cv::Rect, …
```

| Type / call | Meaning |
| --- | --- |
| `Mat` | Image header + data (own or wrap) |
| `create` / `release` / `clone` / `copyTo` | Alloc (PSRAM) / free / deep copy |
| `operator()(Rect)` / `rowRange` / `colRange` | ROI / submatrix |
| `Point` `Point2f` `Size` `Rect` `Scalar` `Range` | Geometry |
| `CV_8UC1` / `CV_8UC2` / `CV_8UC3` | Gray / RGB565 / RGB888 |
| `wrapRgb565` / `wrapGray` / `wrapRgb888` | Zero-copy headers |

Imgproc on `Mat` is still mostly via `ESP32P4_Cv` + raw pointers; Mat is the buffer/ROI layer.

### `ESP32P4_FaceAi`

| Call | Meaning |
| --- | --- |
| `begin(model, db, names)` | Load detector (+ recognizer if MFN present) |
| `run(rgb\|fb, out, max, recognize)` | Detect; optionally match DB |
| `requestEnroll(name)` / `cancelEnroll` | Multi-frame enroll → one feature |
| `clearDb` / `deleteId` / `deleteName` | Manage DB |
| `setName` / `nameOf` / `rosterText` | Names / UI roster |
| `setThresh` / `thresh` | Similarity threshold |
| `draw(...)` | Boxes + labels on RGB565 |
| `featCount` / `lastEnrollStatus` | Status |

DB paths use VFS (`/sdcard/...`). SD_MMC card paths are `/face/...`.

### `ESP32P4_Sd` / `WebFileManager`

| Call | Meaning |
| --- | --- |
| `sd.begin(board\|cfg)` | Mount FAT SD |
| `wfm.setPorts(ui, file).setHomePort(camUi).begin()` | File browser HTTP |
| `wfm.startFileTask()` | Keep UI responsive during transfers |
| `wfm.loop()` | Pump from Arduino `loop()` |

Typical ports: camera `:80`, WFM UI `:82`, WFM transfers `:83`.

### Memory

```cpp
void *p = esp32p4_psram_alloc(bytes);
esp32p4_psram_free(p);
esp32p4_psram_msync(p, bytes);
size_t free_b = esp32p4_psram_free_size();
```

---

## Examples

| Sketch | What it shows |
| --- | --- |
| `01_CamTest` | Capture loop |
| `02_JpegSnapshot` | HW JPEG |
| `04_WiFiMjpeg` | Wi‑Fi MJPEG UI |
| `09_SdCard` | SD mount |
| `10_WiFiMjpegSdCapture` | Capture → `/IMG` |
| `11`–`12` H264 | Record MP4 |
| `14_EthSdBrowser` | WebFileManager |
| `17` / `18_EthH264Record*` | Eth + record + mic (+ files) |
| `19_CvColorBlobs` / `20_EthCvPreview` | HSV blobs / live CV |
| `21_EthFaceWeb` | Eth + face + settings on SD + capture/record + WFM |

Arduino IDE: **File → Examples → ESP32CSI_Vision → …**

---

## Layout

```text
src/
  ESP32CSI_Vision.h     ← umbrella include
  cam/ jpeg/ ppa/ dsp/  ← capture / encode / scale / motion
  stream/               ← MJPEG + UI
  cv/ opencv/           ← imgproc + Mat core
  face/ espdl/          ← Face AI + vendored ESP-DL
  h264/ audio/ sd/      ← record / mic / storage
  wfm/                  ← WebFileManager
examples/               ← Arduino sketches
```

---

## Notes

- Always `cam.release(fb)` after `capture()`.
- Keep MJPEG on `:81` so `/control` on `:80` stays responsive.
- Avoid heavy SD browse/upload while recording.
- Face models must be on SD before `FaceAi.begin()`; WFM can upload them.
- Class prefix stays `ESP32P4_*` for API stability; library name is CSI-oriented.

---

## License

MIT · [Rakib Hasan](https://github.com/thezerohz) ([@thezerohz](https://github.com/thezerohz)) · [repo](https://github.com/thezerohz/ESP32CSI_Vision)
