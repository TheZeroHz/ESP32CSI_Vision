# API Reference

Public API for **ESP32CSI_Vision** v3.25.0. Umbrella: `#include <ESP32CSI_Vision.h>` (or `#include "board_config.h"` in examples).

Enums and structs: [Enums & Types](Enums-and-Types.md). HTTP: [HTTP & Preferences](HTTP-and-Preferences.md). Pins: [Custom boards](../Custom-Boards.md).

**Sketch convention:** declare objects, call methods on them. Model IDs are global (`ESP32P4_DET_DOG_224`, `ESP32P4_FACE_MSRMNP_S8_V1`). Nested `Class::Enum` values exist for C++ but sketches use the `ESP32P4_*` names. `bool` = success. Every `capture()` needs `release(fb)`.

Jump: [board_config](#board_configh) · [Camera](#esp32p4_camera) · [JPEG](#esp32p4_jpeg) · [H.264](#esp32p4_h264) · [PPA](#esp32p4_ppa) · [Img](#esp32p4_img) · [Cv](#esp32p4_cv) · [Dsp](#esp32p4_dsp) · [MJPEG](#esp32p4_mjpegserver) · [WebPreview](#esp32p4_webpreview) · [SD / Storage](#esp32p4_sd--esp32p4_storagepref) · [Mic](#esp32p4_mic) · [Detect](#esp32p4_objectdetect) · [Pose / Seg / Cls / Gesture / Reid / OCR](#pose-seg-cls-gesture-reid-ocr) · [Face](#esp32p4_facedetect--esp32p4_faceai) · [QR](#esp32p4_qr) · [UVC](#esp32p4_uvc) · [V4L2](#esp32p4_v4l2) · [WHO](#esp32p4_whopipeline) · [VisionAi](#esp32p4_visionai) · [Debug](#esp32p4_debug) · [PSRAM](#psram)

---

## `board_config.h`

Lives **next to the sketch**, not in `src/`. Arduino IDE shows it as a tab. Copy `examples/00_BoardConfig/board_config.h`.

| Helper | Returns |
| --- | --- |
| `esp32csi_cam_config()` | `esp32p4_cam_config_t` from `CFG_CAM_*` |
| `esp32csi_sd_config()` | `esp32p4_sd_config_t` from `CFG_SD_*` |
| `esp32csi_mic_config()` | `esp32p4_mic_config_t` from `CFG_MIC_*` |
| `esp32csi_wifi_config()` / `esp32csi_wifi_begin()` | C6 SDIO STA |
| `esp32csi_eth_config()` / `esp32csi_eth_begin()` | RMII PHY |
| `esp32csi_wfm_eth()` | `WfmEthConfig` for `WebFileManager` / `net.beginEthernet(...)` |
| `esp32csi_print_board()` | Serial `CFG:` dump |

```cpp
#include "board_config.h"
ESP32P4_Camera cam;
cam.begin(esp32csi_cam_config());
esp32csi_wifi_begin();
```

Library fallbacks still exist: `esp32p4_cam_config_default()`, `esp32p4_cam_config_board(ESP32P4_BOARD_GUITION_M3)`, `cam.begin(ESP32P4_BOARD_GUITION_M3)`. Prefer the sketch tab.

---

## ESP32P4_Camera

**Header:** `cam/ESP32P4_Camera.h` · **Alias:** `ESP32P4_CSI_Camera`

MIPI CSI + ISP (default), or DVP / SPI / USB-host UVC. Output is `camera_fb_t` in PSRAM. Default format **RGB565**.

### Lifecycle / capture

| Method | Notes |
| --- | --- |
| `bool begin(const esp32p4_cam_config_t &cfg)` | Preferred. Probes sensor, allocs FBs, starts stream. |
| `bool begin(esp32p4_board_t board)` | Library pin preset (Guition default). |
| `void end()` | Stop + free. Safe if never started. |
| `camera_fb_t *capture(uint32_t timeout_ms = 2000)` | Pool slot or `nullptr`. |
| `void release(camera_fb_t *fb)` | Required. |
| `bool startCapture()` / `stopCapture()` | Pause/resume without `end()`. |
| `bool isCaptureStarted()` | |

```cpp
ESP32P4_Camera cam;
if (!cam.begin(esp32csi_cam_config())) { /* fail */ }
camera_fb_t *fb = cam.capture(500);
if (!fb) return;
// fb->buf, fb->width, fb->height, fb->len, fb->format
cam.release(fb);
```

### Format / JPEG capture

| Method | Notes |
| --- | --- |
| `bool setFormat(esp32p4_cam_pixformat_t fmt)` | Restart pipeline. |
| `format()` / `formatName()` | Current FB format. |
| `bool supportsFormat(fmt)` | |
| `bool setJpegQuality(uint8_t q)` | 1–100 when capturing JPEG. |
| `bool setJpegChroma(esp32p4_jpeg_chroma_t c)` | RGB inputs. |

### Sensor / ISP

| Group | Methods |
| --- | --- |
| Mirror / flip | `setHMirror` `setVFlip` (+ getters) |
| AE / AGC | `setAEC` `setAGC` `setExposure` `setGain` `setGainCeiling` `setExposureTime` `setAeTarget` `setAeEvBias` `setAntiFlicker` |
| Picture | `setBrightness` `setContrast` `setSaturation` `setHue` `setSharpness` `setDenoise` |
| AWB / ISP | `setAwb` `setIspAe` `setRedBalance` `setBlueBalance` `ispReady` `ispLuma` `ispEnvLuma` |
| Pattern | `setTestPattern` |
| AF (OV5647) | `afBegin` `afPresent` `setAfPosition` `afScan` `afScore` |
| Identity | `width` `height` `sensorName` `sensorType` `detected` `bus` `busName` `fbCount` `psramOk` |

`setSensor*` names are aliases for the same setters (v4l2-ctl / HTTP).

### Dual camera

```cpp
if (!esp32p4_cam_dual_ok(camA.bus(), camB.bus())) {
  Serial.println(esp32p4_cam_dual_why(camA, camB));
}
```

CSI + CSI is rejected. CSI + DVP/SPI/UVC_HOST is OK (`28_DualCam`).

---

## ESP32P4_Jpeg

**Header:** `jpeg/ESP32P4_Jpeg.h`

| Method | Notes |
| --- | --- |
| `bool begin(max_w, max_h, quality = 45, max_src_bpp = 2)` | Alloc encode/decode scratch. |
| `void end()` | |
| `size_t encode(fb, out, out_cap)` | RGB565 / RGB888 / YUV / GRAY / JPEG copy. |
| `size_t encode(rgb565, w, h, out, out_cap)` | |
| `size_t encode(src, w, h, fmt, out, out_cap)` | Explicit pixformat. |
| `bool decodeInfo(jpg, len, &w, &h)` | |
| `size_t decode(jpg, len, rgb_out, cap, &w, &h)` | → RGB565 |
| `setQuality` / `setChroma` / `clearInput` | |

```cpp
ESP32P4_Jpeg jpeg;
jpeg.begin(cam.width(), cam.height(), 45);
uint8_t *jpg = (uint8_t *)esp32p4_psram_alloc(200 * 1024);
size_t n = jpeg.encode(fb, jpg, 200 * 1024);
```

---

## ESP32P4_H264

**Header:** `h264/ESP32P4_H264.h` — P4 HW Baseline. Prefer `openMp4()` (wall-clock duration, temp Annex-B deleted).

| Method | Notes |
| --- | --- |
| `bool begin(w, h, fps = 15, bitrate = 0)` | |
| `bool begin(const esp32p4_h264_cfg_t &cfg)` | `input_format` default RGB565. |
| `size_t encode(fb, out, cap, int *frame_type = nullptr)` | Also RGB565 pointer overload. |
| `bool openMp4(fs, path, pcm_path = nullptr, pcm_rate = 16000)` | Path must end `.mp4`. Optional PCM → AAC on close. |
| `size_t encodeToFile(fb)` | |
| `void closeFile()` | Remux + delete temp. |
| `setBitrate` `setGop` `setFps` `setQp` `forceIdr` | Runtime RC. |

YUV420 / GRAY8 skip RGB convert. YUV422 UYVY and YUYV go to HW with no colour convert.

Video-only: `11_H264SdRecord`. Video + mic, no stream: **`44_AvSdRecord`**. UI Record: `17` / `30` / `31`.

---

## ESP32P4_Ppa

**Header:** `ppa/ESP32P4_Ppa.h` — P4 SRM (scale / rotate / mirror).

| Method | Notes |
| --- | --- |
| `bool begin()` | Required before scale. |
| `bool scale(src_fb, dst, cap, dst_w, dst_h)` | |
| `bool scaleFit` / `scaleCover` | Letterbox vs crop-fill. |
| `bool rotate90` / `mirror` | |
| `bool scaleRgb565(src, sw, sh, dst, dw, dh)` | |
| `bool rgb565ToGray` / `rgb565ToGrayScale` | |
| `bool fillRect565(...)` | HW fill when possible. |

```cpp
ESP32P4_Ppa ppa;
ppa.begin();
ppa.scale(fb, dst, cap, 320, 240);
```

CPU fallback: `ESP32P4_Img` (`06_PpaScale`).

---

## ESP32P4_Img

**Header:** `img/ESP32P4_Img.h` — software RGB565 helpers. Call on an object in sketches (`img.downsample2x565(...)`).

| Method | Notes |
| --- | --- |
| `rgb565ToRgb888` / `rgb888ToRgb565` | Packed. |
| `luma565` / `histogram565` | |
| `crop565` / `downsample2x565` / `blit565` | |
| `fillRect565(img, w, h, rect, color, thickness = 2)` | |

---

## ESP32P4_Cv

**Header:** `cv/ESP32P4_Cv.h` — OpenCV-style imgproc on RGB565 / GRAY8. PPA used for full-frame gray/scale.

| Method | Notes |
| --- | --- |
| `toGray` / `toGrayScale` | RGB565 → GRAY8 |
| `blur3x3` `threshold` `otsu` `adaptiveThreshold` | |
| `morphologyOpen` / `Close` `erode` `dilate` | |
| `inRangeHsv` `rgb565ToHsv` | H 0..179 |
| `edges` | Sobel + dual threshold |
| `findBlobs` | Needs `label_scratch` (PSRAM) |
| `line` `circle` `putText` `drawBlob` | Annotate RGB565 |

```cpp
ESP32P4_Cv cv;
cv.inRangeHsv((uint16_t *)fb->buf, fb->width, fb->height, mask, lo, hi);
int n = cv.findBlobs(mask, w, h, blobs, 16, 80, labels);
```

`esp_cv::Mat` in `opencv/esp_cv.hpp` wraps the same buffers. Example `19_CvColorBlobs`. In `20_EthCvPreview` use `stream.cvConfig()` — do not declare a global `cv` there.

---

## ESP32P4_Dsp

**Header:** `dsp/ESP32P4_Dsp.h` — frame-diff motion.

| Method | Notes |
| --- | --- |
| `bool begin(w, h, threshold = 25)` | Downsampled gray history. |
| `bool detect(fb, esp32p4_motion_t *out)` | `out->moving`, `roi`. |

Example `05_MotionDetect`.

---

## ESP32P4_MjpegServer

**Header:** `stream/ESP32P4_Mjpeg.h`

Fat Camera UI: a **worker** captures, optional PPA scale, JPEG, MJPEG. You do not call `capture()` in `loop()` unless you also hook overlays.

| Method | Notes |
| --- | --- |
| `bool begin(ESP32P4_Camera *cam, uint16_t port = 80, uint8_t quality = 35)` | UI `port`, stream `port+1`. |
| `void loop()` | HTTP + debug poll. Call every loop. |
| `void end()` | |
| `setQuality` `setFrameSkip` `setFramesize` | Stream enum 0–9. |
| `enableCapture(fs, "/IMG")` | Snapshot JPEG. |
| `enableVideoRecord(fs, &h264, "/VIDEO")` | UI Record → MP4. |
| `enableMic(&mic)` | Waveform + AAC in MP4. |
| `setFilesBrowserPort(port)` | Link to WebFileManager. |
| `setOverlay(...)` | RGB565 annotate before JPEG (does not mutate H.264 FB). |

```cpp
ESP32P4_MjpegServer stream;
esp32csi_wifi_begin();
stream.begin(&cam, 80, 35);   // http://ip/  and  http://ip:81/stream
```

Ports, `/control`, `/status`: [HTTP & Preferences](HTTP-and-Preferences.md).

---

## ESP32P4_WebPreview

**Header:** `stream/ESP32P4_WebPreview.h`

**You** own capture. No hidden worker. Browser: `/` (preview), `/stream`, `/jpg`, `/dets` (whatever JSON you set).

| Method | Notes |
| --- | --- |
| `bool begin(port = 80, quality = 40, max_w = 1920, max_h = 1080)` | |
| `bool present(const camera_fb_t *fb)` | Encode + publish. FB still yours — `release` after. |
| `bool presentRgb565(rgb565, w, h)` | |
| `void setStatusJson(const char *json)` | Served at `/dets`. |
| `void setTitle(const char *title)` | |
| `void loop()` | |

```cpp
ESP32P4_WebPreview preview;
preview.begin(80, 40);
camera_fb_t *fb = cam.capture();
int n = det.infer(fb);
det.draw((uint16_t *)fb->buf, fb->width, fb->height, det.results(), n, det.model());
preview.setStatusJson(json);
preview.present(fb);
cam.release(fb);
preview.loop();
```

Example `43_CamWebModels`.

---

## ESP32P4_Sd / ESP32P4_StoragePref

**Headers:** `sd/ESP32P4_Sd.h`, `storage/ESP32P4_StoragePref.h`

```cpp
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
sd.begin(esp32csi_sd_config());
store.begin(ESP32P4_STORAGE_AUTO, false, &sd);
store.locateModel("/models/p4/espdet_pico_224_224_dog.espdl");
```

| SD | Notes |
| --- | --- |
| `begin(esp32csi_sd_config())` | Preferred. |
| `fs()` | `SD_MMC`. |
| `writeFile` `writeBytes` `readFile` `listDir` `mkdir` `remove` `exists` | |
| `cardType` `totalBytes` `usedBytes` | |

| StoragePref | Notes |
| --- | --- |
| `begin(pref, format_flash, &sd)` | Mounts preferred + extras. |
| `fs()` `vfsRoot()` `vfsPath()` | Arduino vs `fopen` paths. |
| `locateModel(rel)` | Points ESP-DL at whichever volume has the file. |
| `attachToWfm(wfm)` | Extra volumes in the file manager. |

Models also load from FFat / LittleFS / SPIFFS with **no SD**. Partition tip: `app3M_fat9M_16MB`. Long H.264 belongs on microSD.

---

## ESP32P4_Mic

**Header:** `audio/ESP32P4_Mic.h` — ES8311 + I2S.

```cpp
ESP32P4_Mic mic;
mic.begin(esp32csi_mic_config());
mic.poll();   // every loop while recording
```

| Method | Notes |
| --- | --- |
| `begin(esp32csi_mic_config())` | Preferred. |
| `startPcmFile(fs, path)` / `startPcmRam(cap)` | |
| `stopPcmFile` / `pcmBytes` | |
| `setGain(percent)` | 0–100 |
| `copyWave` / `readStream` | UI waveform / AAC source. |

Examples `15_MicSdRecord`, `30_EthLiveAvFiles`.

---

## ESP32P4_ObjectDetect

**Header:** `detect/ESP32P4_ObjectDetect.h`

Pass an image, get boxes + class names. Weights: `/models/p4/*.espdl` on any mounted volume.

| Method | Notes |
| --- | --- |
| `bool begin(ESP32P4_DET_DOG_224)` | Any `ESP32P4_DET_*`. |
| `bool setModel(m)` | Switch weights. |
| `void setScoreThr(float)` | Default 0.25. |
| `int infer(fb)` | Stores up to 32 hits in `results()`. |
| `int detect(fb, out, max_out)` | Caller buffer. |
| `inferRgb888` / `inferJpeg(jpeg, jpg, len)` | |
| `const esp32p4_det_t *results()` | |
| `size_t resultsJson(buf, cap)` | |
| `label(category)` / `modelName` / `modelFile` | |
| `draw(rgb565, w, h, dets, n, model, color)` | Boxes + names. |

```cpp
ESP32P4_ObjectDetect det;
det.begin(ESP32P4_DET_DOG_224);
int n = det.infer(fb);
det.resultsJson(json, sizeof(json));
det.draw((uint16_t *)fb->buf, fb->width, fb->height, det.results(), n, det.model());
```

Serial-only: `42_DetectApi`. Web: `32`–`36`, `43`.

---

## Pose, Seg, Cls, Gesture, Reid, OCR

Same pattern: `begin` → `detect`/`run`/`classify` on RGB565 or `camera_fb_t` → optional `draw`.

| Class | `begin` | Infer | Weights |
| --- | --- | --- | --- |
| `ESP32P4_Pose` | `ESP32P4_POSE_YOLO11N_V2` | `detect` → `esp32p4_pose_t` (COCO-17) | `coco_pose_yolo11n_pose_s8_v2.espdl` |
| `ESP32P4_Seg` | default YOLO11n-Seg | `detect` → masks valid until next call | `coco_seg_yolo11n_seg_s8_v1.espdl` |
| `ESP32P4_Cls` | `begin(topk = 5)` | `classify` → ImageNet top-k | `imagenet_cls_mobilenetv2_s8_v1.espdl` |
| `ESP32P4_Gesture` | `begin()` | hand box + 8-class label | hand Pico + gesture MobileNet |
| `ESP32P4_Reid` | `begin("/sdcard/reid.db")` | `run` / `enroll` | pedestrian Pico + OSNet |
| `ESP32P4_Ocr` | `ESP32P4_OCR_REC_S16`, `ESP32P4_OCR_SHORT` | `run` → quads + `text` | PaddleOCR v6 det + rec |
| `ESP32P4_Speaker` | `begin(6)` | `embedPcm` / `embedWav` — **caller `free()`s** embedding | `sv_tdnn_tiny_3s/6s.espdl` |

Examples `37`–`41`. Speaker needs ESP-DL audio (`ESP32P4_ESPDL_ENABLE_SPEAKER`).

---

## ESP32P4_FaceDetect / ESP32P4_FaceAi

**Headers:** `face/ESP32P4_FaceDetect.h`, `face/ESP32P4_FaceAi.h`

Vendored ESP-DL — **Arduino and IDF**. Extra IDF samples: `idf_examples/08_FaceDetect`, `21_EthFaceWeb`.

| FaceDetect | Notes |
| --- | --- |
| `begin(ESP32P4_FACE_MSRMNP_S8_V1)` | Also Pico 224 / 416. |
| `int detect(fb, out, max_out)` | Boxes + 5 landmarks. |
| `draw(...)` | |

| FaceAi | Notes |
| --- | --- |
| `begin(det_model, db_path, names_path, feat = MFN_S8_V1)` | `db_path = nullptr` → detect-only. |
| `int run(fb, out, max_out, recognize = true)` | Enroll and recognize are mutually exclusive. |
| `requestEnroll(name)` / `cancelEnroll` | N samples then one feature. |
| `clearDb` `deleteId` `deleteName` `setName` | |
| `draw(...)` | |

```cpp
ESP32P4_FaceAi face;
face.begin(ESP32P4_FACE_MSRMNP_S8_V1, "/sdcard/face.db", "/sdcard/face_names.txt");
int n = face.run(fb, ids, 8);
face.draw((uint16_t *)fb->buf, fb->width, fb->height, ids, n);
```

---

## ESP32P4_Qr

**Header:** `qr/ESP32P4_Qr.h`

| Method | Notes |
| --- | --- |
| `bool begin(max_w = 640, max_h = 480, try_downscale = true)` | |
| `int scan(rgb565, w, h, out, max_out)` | |
| `setFormats(mask)` | ZXing format bits. |
| `draw(...)` | Quad overlay. |

Example `22_EthQrWeb`.

---

## ESP32P4_Uvc

**Header:** `uvc/ESP32P4_Uvc.h` — USB **gadget** webcam (CSI → HW JPEG → PC). Not V4L2. Not host UVC.

Declare **global** so TinyUSB registers before CDC. Board: USB-OTG; prefer CDC On Boot **Disabled**.

```cpp
ESP32P4_Camera cam;
ESP32P4_Uvc uvc;          // global
void setup() {
  cam.begin(esp32csi_cam_config());
  uvc.begin(&cam, 45);
}
```

Do not combine with `ESP32P4_CAM_BUS_UVC_HOST`. Example `23_UsbUvc`. Host webcam → `camera_fb_t`: `26_UsbHostUvc`.

---

## ESP32P4_V4l2

**Header:** `v4l2/ESP32P4_V4l2.h` — POSIX node on the **same** `ESP32P4_Camera`. Does not replace `capture()`.

| Method | Notes |
| --- | --- |
| `bool begin(&cam, path = nullptr)` | Default `/dev/video0` CSI, `/dev/video2` DVP, … |
| `int fd()` | `-1` if VFS off. |
| `int ioctl(request, arg)` | |
| `setCtrl` / `getCtrl` / `listCtrls` / `listFormats` | |
| `int ctl("--list-ctrls")` | v4l2-ctl-style string. |
| `dqbuf` / `qbuf` | Use these **or** `cam.capture()`, not both. |

Example `27_V4l2Ctl`. Do not also start Arduino `ESP_Video` on the same CSI host.

---

## ESP32P4_WhoPipeline

**Header:** `who/ESP32P4_Who.h` — async capture queue (no ESP-DL).

| Method | Notes |
| --- | --- |
| `bool begin(&cam, queue_len = 2)` | |
| `void onFrame(cb, ctx)` | |
| `bool waitFrame(esp32p4_who_fb_t *out, timeout_ms)` | |

For models use `FaceAi` / `ObjectDetect`, not this queue. Example `07_WhoPipeline`.

---

## ESP32P4_VisionAi

**Header:** `vision/ESP32P4_VisionAi.h` — letterbox RGB565 → model RGB888, box remap, NMS, result structs.

```cpp
ESP32P4_VisionAi vai;
vai.letterboxRgb565(rgb565, sw, sh, dst, model_w, model_h, &lb);
```

Used internally by detect wrappers; also for custom ESP-DL (`19_CvColorBlobs`).

---

## ESP32P4_Debug

**Header:** `debug/ESP32P4_Debug.h`

```cpp
ESP32P4_Debug dbg;
dbg.begin("31_WiFiLiveAvFiles", ESP32P4_DBG_LIVE);
dbg.poll();
```

NVS namespace `csi_dbg` overrides the sketch mask. Serial `d` / `d=<mask>` / `d=r`. MJPEG UI: `GET /debug`. Bits: [Enums](Enums-and-Types.md#debug-mask).

---

## PSRAM

**Header:** `mem/ESP32P4_Psram.h`

| Function | Notes |
| --- | --- |
| `esp32p4_psram_alloc(bytes, align = 128)` | Cache-aligned. |
| `esp32p4_psram_free(ptr)` | |
| `esp32p4_psram_msync` / `writeback` | After DMA write / before DMA read. |
| `esp32p4_psram_available` / `free_size` | |
| `esp32p4_prefer_psram()` | `malloc` ≥ 1 KB prefers PSRAM. |

---

## WebFileManager

**Header:** `wfm/WebFileManager.h` — HTTP file browser (typically port 82). Ethernet helper: `esp32csi_wfm_eth()`. Camera UI: `stream.setFilesBrowserPort(82)`. Examples `14`, `18`, `30`, `31`.

---

## Compile-time knobs

| Macro | Effect |
| --- | --- |
| `ESP32P4_UVC_WIDTH` / `HEIGHT` / `FPS` | Gadget descriptor fallback |
| `ESP32P4_FACE_MAX_NAMES` | Default 48 |
| `ESP32P4_FACE_ENROLL_SAMPLES` | Default 5 |
| `ESP32P4_ESPDL_ENABLE_SPEAKER` | Speaker verification |

← [Getting Started](Getting-Started.md) · [Home](Home.md) →
