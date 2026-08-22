# Examples Map

Arduino: **File → Examples → ESP32CSI_Vision → …**  
Each folder has its own `board_config.h`. Pins are not in `src/`.

Guide: [Custom-Boards.md](../Custom-Boards.md) → flash **`00_BoardConfig`** → read Serial `CFG:`.

ESP-IDF extras: `idf_examples/08_FaceDetect`, `09_CocoDetect`, `21_EthFaceWeb`.

---

## Learning path

| Step | Sketch | You learn |
| --- | --- | --- |
| 0 | `00_BoardConfig` | Dump cam / SD / mic / Wi-Fi / ETH GPIOs |
| 1 | `01_CamTest` | `capture` / `release` |
| 2 | `02_JpegSnapshot` | HW JPEG encode |
| 3 | `03_JpegDecode` | Decode without a camera |
| 4 | `04_WiFiMjpeg` | C6 Wi-Fi + fat Camera UI (`:80` / `:81`) |
| 5 | `05_MotionDetect` | Frame-diff ROI |
| 6 | `06_PpaScale` | PPA scale + CPU `downsample2x565` |
| 7 | `07_WhoPipeline` | Async capture queue |
| 9 | `09_SdCard` | SDMMC via `esp32csi_sd_config()` |

Then pick a track:

| Track | Sketches |
| --- | --- |
| Other buses | `24_DvpCam` `25_SpiCam` `26_UsbHostUvc` `23_UsbUvc` `27_V4l2Ctl` `28_DualCam` |
| Storage + AV | `10` `11` `12` `14`–`18` `29`–`31` `44` |
| CV / QR | `19` `20` `22` |
| Detect zoo (Eth MJPEG tabs) | `32`–`41` |
| You own the FB | `42_DetectApi` (Serial JSON) `43_CamWebModels` (preview + `/dets`) |

---

## Catalog

| # | Sketch | Objects | What it proves |
| --- | --- | --- | --- |
| 00 | `00_BoardConfig` | `esp32csi_print_board` | Wiring dump + optional cam probe |
| 01 | `01_CamTest` | `Camera` `Debug` | Capture loop |
| 02 | `02_JpegSnapshot` | `Camera` `Jpeg` | `jpeg.encode(fb, …)` |
| 03 | `03_JpegDecode` | `Jpeg` | `decodeInfo` / `decode` — no CSI |
| 04 | `04_WiFiMjpeg` | `Camera` `MjpegServer` | Full UI + dual-port stream |
| 05 | `05_MotionDetect` | `Camera` `Dsp` | `dsp.detect(fb, &m)` |
| 06 | `06_PpaScale` | `Camera` `Ppa` `Img` | HW scale + CPU fallback |
| 07 | `07_WhoPipeline` | `Camera` `WhoPipeline` | `onFrame` / `waitFrame` |
| 09 | `09_SdCard` | `Sd` | R/W on SDMMC |
| 10 | `10_WiFiMjpegSdCapture` | MJPEG + Sd | UI snapshot → `/IMG` |
| 11 | `11_H264SdRecord` | `H264` `Sd` | MP4 video-only (no stream) |
| 12 | `12_WiFiH264Record` | Wi-Fi + H264 | Record from UI |
| 13 | `13_EthernetTest` | ETH | Link + IP |
| 14 | `14_EthSdBrowser` | ETH + WFM + Sd | File explorer |
| 15 | `15_MicSdRecord` | `Mic` `Sd` | WAV (`CFG_MIC_*`) |
| 16 | `16_MicSdWebFileManager` | Mic + WFM | GPIO / Serial `r` → `/Recording` |
| 17 | `17_EthH264Record` | Eth MJPEG + H264 + Mic | Live + Capture Img + MP4 |
| 18 | `18_EthH264RecordFiles` | 17 + WFM | UI `:80` ↔ files `:82` |
| 19 | `19_CvColorBlobs` | `Cv` `VisionAi` | HSV blobs + letterbox |
| 20 | `20_EthCvPreview` | Eth MJPEG + `stream.cvConfig()` | Blob overlay on the stream buffer |
| 21 | `21_EthFaceWeb` | Eth MJPEG + `FaceAi` | Detect / enroll / recognize |
| 22 | `22_EthQrWeb` | Eth MJPEG + `Qr` | QR / barcode overlay |
| 23 | `23_UsbUvc` | `Uvc` | PC webcam gadget |
| 24 | `24_DvpCam` | `Camera` DVP | OV2640 parallel RGB565 |
| 25 | `25_SpiCam` | `Camera` SPI | SP0A39 GRAY8 |
| 26 | `26_UsbHostUvc` | `Camera` UVC_HOST | USB webcam → `camera_fb_t` |
| 27 | `27_V4l2Ctl` | `V4l2` | `/dev/video0` + Serial v4l2-ctl |
| 28 | `28_DualCam` | two `Camera` | CSI + DVP/SPI/host UVC (not two CSI) |
| 29 | `29_WiFiMjpegWaveshareNano` | Camera Sd Mic MJPEG H264 | Waveshare Nano pin set |
| 30 | `30_EthLiveAvFiles` | ETH + live AV + WFM | `CFG_ETH_*` |
| 31 | `31_WiFiLiveAvFiles` | C6 Wi-Fi + live AV + WFM | `CFG_WIFI_*` |
| 32 | `32_EthCocoWeb` | `ObjectDetect` | COCO / YOLO26 / Pico zoo |
| 33 | `33_EthYolo26Web` | YOLO26n | 640 / 512 |
| 34–36 | Cat / Dog / Hand | Pico | Single-class |
| 37 | `37_EthPoseWeb` | `Pose` | COCO-17 skeletons |
| 38 | `38_EthSegWeb` | `Seg` | YOLO11n-Seg masks |
| 39 | `39_EthGestureWeb` | `Gesture` | Hand + 8 classes |
| 40 | `40_EthClsWeb` | `Cls` | ImageNet top-5 |
| 41 | `41_EthOcrWeb` | `Ocr` | PaddleOCR v6 |
| 42 | `42_DetectApi` | `ObjectDetect` | `infer` → Serial JSON (SD optional) |
| 43 | `43_CamWebModels` | `WebPreview` + `ObjectDetect` | You `capture` → `infer` → `present` |
| 44 | `44_AvSdRecord` | `Camera` `H264` `Mic` `Sd` | MP4 + AAC, **no stream** |

---

## Snippets (object style)

### 01 — capture

```cpp
#include "board_config.h"
ESP32P4_Camera cam;
ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  dbg.begin("01_CamTest", ESP32P4_DBG_CAM);
  cam.begin(esp32csi_cam_config());
}

void loop() {
  dbg.poll();
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) return;
  Serial.printf("%ux%u\n", fb->width, fb->height);
  cam.release(fb);
}
```

### 02 — JPEG

```cpp
ESP32P4_Jpeg jpeg;
jpeg.begin(cam.width(), cam.height(), 45);
uint8_t *jpg = (uint8_t *)esp32p4_psram_alloc(200 * 1024);
size_t n = jpeg.encode(fb, jpg, 200 * 1024);
```

### 04 — Wi-Fi MJPEG

SSID / C6 pins: `CFG_WIFI_*` in `board_config.h`. Live knobs: [HTTP](HTTP-and-Preferences.md).

```cpp
esp32csi_wifi_begin();
stream.begin(&cam, 80, 35);   // UI :80  stream :81
```

```bash
python examples/04_WiFiMjpeg/cam_wifi_viewer.py <ip> 81
```

### 05 — motion

```cpp
ESP32P4_Dsp dsp;
dsp.begin(fb->width, fb->height, 25);
esp32p4_motion_t m;
dsp.detect(fb, &m);
```

### 06 — PPA

```cpp
ESP32P4_Ppa ppa;
ESP32P4_Img img;
ppa.begin();
if (!ppa.scale(fb, dst, cap, dw, dh))
  img.downsample2x565((uint16_t *)fb->buf, fb->width, fb->height, (uint16_t *)dst);
```

### 30 — Ethernet live AV

PHY: `CFG_ETH_*`. Same UI as 04 plus Capture Img, Record MP4, Files, mic.

```cpp
esp32csi_eth_begin();
stream.begin(&cam, 80, 35);
```

### 42 / 43 — detect

Copy `models/espdl/p4/*.espdl` to `/models/p4/` on SD or FFat.

```cpp
ESP32P4_ObjectDetect det;
det.begin(ESP32P4_DET_DOG_224);
int n = det.infer(fb);
det.resultsJson(json, sizeof(json));
det.draw((uint16_t *)fb->buf, fb->width, fb->height, det.results(), n, det.model());
preview.present(fb);
cam.release(fb);
```

### 44 — video + mic, no stream

```cpp
mic.begin(esp32csi_mic_config());
mic.startPcmRam();
h264.openMp4(&store.fs(), "/VIDEO/VID_00001.mp4");
// capture → ppa.scale → encodeToFile; call mic.poll() often
mic.stopPcmFile();
h264.setPcmRam(mic.pcmRam(), mic.pcmRamBytes(), (uint32_t)mic.sampleRate());
h264.closeFile();   // remux MP4 + AAC
```

---

## Board config (all examples)

1. Edit **`board_config.h`** in **that** example folder.
2. Flash **`00_BoardConfig`** and read `CFG:`.
3. Run `01` / `04` / `09` / `15` / `30` / `43` / `44` as needed.

Copies are independent. Duplicate the file if you want the same pins everywhere.

← [API Reference](API-Reference.md) · [Home](Home.md) →
