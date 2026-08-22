# Enums & Types

All of these are in the umbrella header (via `board_config.h` or `<ESP32CSI_Vision.h>`).

Jump: [Boards](#boards) · [Bus](#camera-bus) · [Sensors](#sensors) · [Capture size](#native-framesize) · [Pixformat](#pixel-format) · [Stream size](#stream-framesize) · [Detect models](#object-detect-models) · [Face / pose / OCR](#face-pose-ocr) · [Debug](#debug-mask) · [Storage](#storage) · [Structs](#structs)

---

## Boards

`esp32p4_board_t` — **label only**. GPIOs come from `board_config.h`.

| Enum | Value | Typical carrier |
| --- | :---: | --- |
| `ESP32P4_BOARD_GUITION_M3` | 0 | Guition JC-ESP32P4-M3 |
| `ESP32P4_BOARD_WAVESHARE_NANO` | 1 | Waveshare ESP32-P4 Nano |
| `ESP32P4_BOARD_FUNCTION_EV` | 2 | Espressif Function EV |
| `ESP32P4_BOARD_CUSTOM` | 3 | Default in examples — pins from `CFG_*` |

```cpp
esp32p4_cam_config_t cfg = esp32csi_cam_config();  // board_config.h
cam.begin(cfg);
```

`cam.begin(ESP32P4_BOARD_GUITION_M3)` still works (library pin preset). Prefer `esp32csi_cam_config()` so the sketch tab is the source of truth.

---

## Camera bus

`esp32p4_cam_bus_t`

| Enum | Value | Example |
| --- | :---: | --- |
| `ESP32P4_CAM_BUS_CSI` | 0 | Default MIPI |
| `ESP32P4_CAM_BUS_DVP` | 1 | `24_DvpCam` (OV2640) |
| `ESP32P4_CAM_BUS_SPI` | 2 | `25_SpiCam` (SP0A39) |
| `ESP32P4_CAM_BUS_UVC_HOST` | 3 | `26_UsbHostUvc` |

Two CSI objects are not allowed. CSI + DVP/SPI/UVC_HOST is OK (`esp32p4_cam_dual_ok`). USB gadget (`ESP32P4_Uvc`) cannot share the PHY with `UVC_HOST`.

---

## Sensors

`esp32p4_cam_sensor_t` — `ESP32P4_SENSOR_AUTO` (0) probes SCCB. Named IDs skip probe / force a driver.

Set in `board_config.h`:

```cpp
#define CFG_CAM_SENSOR ESP32P4_SENSOR_OV5647
```

Full matrix, IPA coverage, and status: [CSI-Cameras.md](../CSI-Cameras.md).

---

## Native framesize

`esp32p4_cam_framesize_t` — CSI / ISP **capture** size (`cfg.frame_size`).

| Enum | Value | Typical pixels |
| --- | :---: | --- |
| `ESP32P4_FRAMESIZE_AUTO` | 0 | Sensor default |
| `ESP32P4_FRAMESIZE_VGA` | 1 | 640×480 |
| `ESP32P4_FRAMESIZE_800X640` | 2 | 800×640 (OV5647 RAW8 binning) |
| `ESP32P4_FRAMESIZE_SVGA` | 3 | 800×600 |
| `ESP32P4_FRAMESIZE_HD` | 4 | 1280×720 |
| `ESP32P4_FRAMESIZE_SXGA` | 5 | 1280×960 |
| `ESP32P4_FRAMESIZE_1080P` | 6 | 1920×1080 |
| `ESP32P4_FRAMESIZE_2304X1296` | 7 | IMX708 binned |
| `ESP32P4_FRAMESIZE_QXGA` | 8 | 1600×1200 |
| `ESP32P4_FRAMESIZE_5MP` | 9 | ~2592×1944 |

```cpp
cfg.frame_size = ESP32P4_FRAMESIZE_800X640;
```

ISP RGB max is 1920×1080. Larger RAW modes stay RAW10.

---

## Pixel format

`esp32p4_cam_pixformat_t` — **framebuffer** format (`cfg.pixel_format` / `fb->format`). Not the MIPI packet type in [CSI-Cameras.md](../CSI-Cameras.md).

| Enum | Value | Bytes/px | Notes |
| --- | :---: | :---: | --- |
| `ESP32P4_PIXFORMAT_RGB565` | 0 | 2 | Default. MJPEG UI, face, CV, QR, default H.264, UVC JPEG. |
| `ESP32P4_PIXFORMAT_RAW10` | 1 | 2 | ISP bypass, unpacked |
| `ESP32P4_PIXFORMAT_RAW8` | 2 | 1 | ISP bypass |
| `ESP32P4_PIXFORMAT_RGB888` | 3 | 3 | ISP RGB888 |
| `ESP32P4_PIXFORMAT_YUV422` | 4 | 2 | ISP UYVY. H.264 HW input |
| `ESP32P4_PIXFORMAT_YUV420` | 5 | 1.5 | ISP I420. H.264 packs to HW YUV |
| `ESP32P4_PIXFORMAT_GRAY8` | 6 | 1 | Y plane; `fb->len = w*h` |
| `ESP32P4_PIXFORMAT_JPEG` | 7 | compressed | HW JPEG; `fb->len` is JFIF size |
| `ESP32P4_PIXFORMAT_YUYV` | 8 | 2 | Packed YUYV |
| `ESP32P4_PIXFORMAT_RAW12` | 9 | 2 | Enum only; resolve uses RAW10 |

```cpp
cfg.pixel_format = ESP32P4_PIXFORMAT_RGB565;
cam.setFormat(ESP32P4_PIXFORMAT_JPEG);
cam.setJpegQuality(40);   // 1–100
```

JPEG chroma (`esp32p4_jpeg_chroma_t`): `ESP32P4_JPEG_CHROMA_AUTO` (RGB→422), `YUV444`, `YUV422`, `YUV420`. `jpeg.setChroma(...)` / `cam.setJpegChroma(...)`.

V4L2 fourcc on the same FB: `ESP32P4_V4l2` (`RGBP`, `RGB3`, `UYVY`, `YUYV`, `YU12`, `GREY`, `JPEG`). Example `27_V4l2Ctl`.

---

## Stream framesize

`esp32p4_stream_framesize_t` — MJPEG **output** size (PPA crop then scale). Runtime only. Every size is a multiple of 16 (JPEG MCU).

| Enum | Value | Output |
| --- | :---: | --- |
| `ESP32P4_STREAM_FHD` | 0 | 1920×1072 |
| `ESP32P4_STREAM_HD` | 1 | 1280×720 |
| `ESP32P4_STREAM_XGA` | 2 | 1024×576 |
| `ESP32P4_STREAM_SVGA` | 3 | 800×640 |
| `ESP32P4_STREAM_VGA` | 4 | 640×480 |
| `ESP32P4_STREAM_HVGA` | 5 | 480×320 |
| `ESP32P4_STREAM_CIF` | 6 | 400×288 |
| `ESP32P4_STREAM_QVGA` | 7 | 320×240 |
| `ESP32P4_STREAM_HQVGA` | 8 | 240×176 |
| `ESP32P4_STREAM_QQVGA` | 9 | 160×128 |

Aliases: `ESP32P4_STREAM_NATIVE` = FHD, `ESP32P4_STREAM_HALF` / `720ISH` = HD.

```cpp
stream.setFramesize(ESP32P4_STREAM_QVGA);
// HTTP: GET /control?var=framesize&val=7
```

---

## Object-detect models

Global IDs for sketches (`esp32p4_det_model_t`). Nested `ESP32P4_ObjectDetect::Model` has the same numeric values.

| Enum | Value | Weights (`/models/p4/`) |
| --- | :---: | --- |
| `ESP32P4_DET_COCO_YOLO11N` | 0 | `coco_detect_yolo11n_s8_v1.espdl` |
| `ESP32P4_DET_COCO_YOLO11N_320` | 1 | `coco_detect_yolo11n_320_s8_v1.espdl` |
| `ESP32P4_DET_PEDESTRIAN_PICO` | 2 | `pedestrian_detect_pico_s8_v1.espdl` |
| `ESP32P4_DET_CAT_224` | 3 | `espdet_pico_224_224_cat.espdl` |
| `ESP32P4_DET_CAT_416` | 4 | `espdet_pico_416_416_cat.espdl` |
| `ESP32P4_DET_DOG_224` | 5 | `espdet_pico_224_224_dog.espdl` |
| `ESP32P4_DET_DOG_416` | 6 | `espdet_pico_416_416_dog.espdl` |
| `ESP32P4_DET_HAND_224` | 7 | `espdet_pico_224_224_hand.espdl` |
| `ESP32P4_DET_YOLO26_640` | 8 | `yolo26n_640_s8_p4.espdl` |
| `ESP32P4_DET_YOLO26_512` | 9 | `yolo26n_512_s8_p4.espdl` |

```cpp
det.begin(ESP32P4_DET_DOG_224);
```

---

## Face, pose, OCR

| Kind | Sketch enum | Nested equivalent |
| --- | --- | --- |
| Face detect | `ESP32P4_FACE_MSRMNP_S8_V1` (0) | `ESP32P4_FaceDetect::MSRMNP_S8_V1` |
| | `ESP32P4_FACE_ESPDET_PICO_224` (1) | `ESPDET_PICO_224` |
| | `ESP32P4_FACE_ESPDET_PICO_416` (2) | `ESPDET_PICO_416` |
| Pose | `ESP32P4_POSE_YOLO11N_V1` (0) | `ESP32P4_Pose::YOLO11N_POSE_V1` |
| | `ESP32P4_POSE_YOLO11N_V2` (1) | `YOLO11N_POSE_V2` (default) |
| OCR rec | `ESP32P4_OCR_REC_S8` / `REC_S16` | `ESP32P4_Ocr::REC_S8` / `REC_S16` |
| OCR mode | `ESP32P4_OCR_SHORT` (48×320) / `DUAL` (+ 48×640) | `SHORT` / `DUAL` |

Face feature (recognition, nested only): `ESP32P4_FaceAi::MFN_S8_V1` (default) or `MBF_S8_V1`.

```cpp
det.begin(ESP32P4_FACE_MSRMNP_S8_V1);
pose.begin(ESP32P4_POSE_YOLO11N_V2);
ocr.begin(ESP32P4_OCR_REC_S16, ESP32P4_OCR_SHORT);
```

CV threshold: `ESP32P4_THRESH_BINARY` / `ESP32P4_THRESH_BINARY_INV`.

---

## Debug mask

`esp32p4_dbg_bit_t`

| Bit | Name | Value |
| --- | --- | ---: |
| cam | `ESP32P4_DBG_CAM` | 1 |
| ppa | `ESP32P4_DBG_PPA` | 2 |
| jpeg | `ESP32P4_DBG_JPEG` | 4 |
| stream | `ESP32P4_DBG_STREAM` | 8 |
| wifi | `ESP32P4_DBG_WIFI` | 16 |
| audio | `ESP32P4_DBG_AUDIO` | 32 |
| h264 | `ESP32P4_DBG_H264` | 64 |
| sd | `ESP32P4_DBG_SD` | 128 |
| isp | `ESP32P4_DBG_ISP` | 256 |
| net | `ESP32P4_DBG_NET` | 512 |
| live | `ESP32P4_DBG_LIVE` | cam\|ppa\|jpeg\|stream\|wifi\|net |
| all | `ESP32P4_DBG_ALL` | 0x3FF |

```cpp
ESP32P4_Debug dbg;
dbg.begin("04_WiFiMjpeg", ESP32P4_DBG_LIVE);
dbg.poll();   // Serial: d  dump, d=543  LIVE, d=r  restore
```

---

## Storage

`esp32p4_storage_kind_t`

| Enum | Value | Volume |
| --- | :---: | --- |
| `ESP32P4_STORAGE_AUTO` | 0 | SD → FFat → LittleFS → SPIFFS |
| `ESP32P4_STORAGE_SD` | 1 | SDMMC (`ESP32P4_Sd`) |
| `ESP32P4_STORAGE_FFAT` | 2 | FFat |
| `ESP32P4_STORAGE_LITTLEFS` | 3 | LittleFS |
| `ESP32P4_STORAGE_SPIFFS` | 4 | SPIFFS |

Arduino FS paths are root-relative (`/models/p4/…`). ESP-DL `fopen` needs the VFS root (`/sdcard`, `/ffat`, …) — `store.vfsPath()` / `store.locateModel()`.

---

## Structs

### `camera_fb_t`

```cpp
struct camera_fb_t {
  uint8_t *buf;
  size_t len;
  uint16_t width;
  uint16_t height;
  esp32p4_cam_pixformat_t format;
  uint32_t timestamp_us;
};
```

### `esp32p4_cam_config_t`

Pins + `sensor`, `frame_size`, `pixel_format`, `fb_count`, `bus`, `dvp` / `spi` / `uvc`. Built by `esp32csi_cam_config()`. Fields: [API Reference](API-Reference.md#esp32p4_camera).

### Detect / CV

```cpp
struct esp32p4_rect_t { int x, y, w, h; };

struct esp32p4_det_t {
  float score;          // 0..1
  int x, y, w, h;       // top-left + size, source pixels
  int category;
  const char *label;    // static string — do not free
};

struct esp32p4_pose_t { esp32p4_det_t box; esp32p4_keypoint_t kp[17]; };
struct esp32p4_seg_t  { esp32p4_det_t box; const uint8_t *mask; int mask_w, mask_h; };
struct esp32p4_cls_t  { const char *label; float score; };
struct esp32p4_gesture_t { esp32p4_det_t hand; const char *label; float score; };
struct esp32p4_reid_t { esp32p4_det_t box; int id; float similarity; };
struct esp32p4_ocr_t  { int points[8]; float score; char text[96]; };

struct esp32p4_face_t {
  float score;
  int x, y, w, h;
  int landmarks[5][2];  // L eye, L mouth, nose, R eye, R mouth
  bool has_landmarks;
};
struct esp32p4_face_id_t {
  esp32p4_face_t face;
  int id;               // enrolled, or -1
  float similarity;
  char name[24];
};

struct esp32p4_motion_t {
  bool moving;
  uint32_t changed, total;
  esp32p4_rect_t roi;
};

struct esp32p4_blob_t { esp32p4_rect_t box; int area, cx, cy; };
struct esp32p4_hsv_t  { uint8_t h, s, v; };  // H 0..179
```

### Type alias

```cpp
using ESP32P4_CSI_Camera = ESP32P4_Camera;
```

← [API Reference](API-Reference.md) · [Home](Home.md) →
