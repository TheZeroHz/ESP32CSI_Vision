# Changelog

## 3.25.0

**Board config beside each example (not in `src/`)**

- Deleted `src/ESP32CSI_AppConfig.h`. Every Arduino sketch has `board_config.h` in its folder (Arduino IDE tab).
- That file lists every camera sensor / bus / framesize / pixel format, plus SDMMC, ES8311, C6 Wi-Fi, and Ethernet PHY options.
- Examples `#include "board_config.h"` then call `esp32csi_cam_config()` / `sd` / `mic` / `wifi` / `eth`. Guide: `docs/Custom-Boards.md`.
- Programmer docs: README is a landing page; wiki API / enums / examples / HTTP match objects, `ESP32P4_DET_*` model IDs, and current framesize enums.
- Example `44_AvSdRecord`: CSI + ES8311 → one `.mp4` with AAC, no Wi-Fi / Ethernet / MJPEG.
- Missing ESP-DL weights no longer crash `begin()` (Seg/Pose/Cls/Gesture/OCR/Face/ObjectDetect return false). Camera UI stays up.
- YOLO11n-Seg: load weights before CSI FBs, HD + 2 framebuffers, no panic if PSRAM slab is too small.
- Example `38_EthSegWeb`: run YOLO11n-Seg on a side task (it takes ~3s). Doing it in the MJPEG hook froze the preview on the first dark frame (`sent 1`).

## 3.24.0

**Any-vendor ESP32-P4 wiring**

- `src/ESP32CSI_AppConfig.h` always lists camera, SDMMC, ES8311, C6 Wi-Fi, and Ethernet GPIOs. Default board label is `CUSTOM`.
- Guide: `docs/Custom-Boards.md`. Example `00_BoardConfig` prints the resolved map.
- Examples no longer assume Guition in comments or hidden `begin(BOARD)` pins.

## 3.23.0

**Vendor board config + sketch-owned web preview**

- `src/ESP32CSI_AppConfig.h` is the one file other board companies edit: CSI pins, sensor, framesize, SD, mic, C6 Wi-Fi, Ethernet PHY. Examples call `esp32csi_cam_config()` / `esp32csi_wifi_begin()` instead of hiding pins in `begin(BOARD)`.
- `ESP32P4_WebPreview`: you `capture()` → `present(fb)` → browser `/stream` + `/dets` JSON. No hidden capture worker.
- Example `43_CamWebModels`: camera FB → `det.infer(fb)` → draw → preview. `42_DetectApi` stays Serial-only JSON.
- `Camera::begin(cfg)` prints the resolved pin / sensor / size config on Serial.

## 3.22.0

**Sony IMX477 + IMX462 CSI (1080p RAW10)**

- **IMX477** / IMX378: chip ID `0x0016` (`0x0477` / `0x0378`), 2-lane RAW10. Default 1920×1080 crop (P4 ISP max); `FRAMESIZE_HD` is 1332×990 4×4-binned. Exposure / analogue gain / flip.
- **IMX462** (IMX290 / IMX327 family): 2-lane RAW10 1080p or 720p. STARVIS global init + 10-bit CSI. Typical modules use 37.125 MHz onboard (`cfg.xclk = -1`).
- Register maps follow the public Linux `imx477.c` / `imx290.c` drivers, rewritten for 10-bit CSI and P4 1080p.

## 3.21.0

**No-SD storage + detect API for other projects**

- Storage is SD-optional: `AUTO` (and even `STORAGE_SD`) falls back to FFat → LittleFS → SPIFFS if no card. Extra volumes are not probed twice.
- ES8311 I²C: `esp32p4_mic_config_t::wire` selects `Wire` (default) or `Wire1` ([#2](https://github.com/TheZeroHz/ESP32CSI_Vision/issues/2)).
- WFM hides leftover `.rec_work` temps; they are swept when video record starts.
- `ESP32P4_ObjectDetect`: pass RGB565 / RGB888 / JPEG / `camera_fb_t`, get `esp32p4_det_t` with **label, class id, score, x,y,w,h**. `infer()` + `results()` + `resultsJson()` / `toJson()`.
- Example `42_DetectApi`. JSON helpers on `ESP32P4_VisionAi` for dets / cls / ocr / gesture.

## 3.20.0

**Full ESP-DL model zoo** (face + COCO + everything in [esp-dl/models](https://github.com/espressif/esp-dl/tree/master/models)):

- Vendored: cat/dog/hand detect, YOLO26, coco_pose, coco_seg, imagenet_cls, hand_gesture, person_reid, pp_ocr_v6, speaker_verification, color/motion detect, feat_database (+ classification vision module)
- `ESP32P4_ObjectDetect` models: COCO YOLO11n 640/320, pedestrian, cat/dog/hand Pico, YOLO26n 640/512
- Arduino wrappers for the rest of the zoo: `ESP32P4_Pose`, `ESP32P4_Seg`, `ESP32P4_Cls`, `ESP32P4_Gesture`, `ESP32P4_Reid`, `ESP32P4_Ocr`, `ESP32P4_Speaker`
- Face feat **MBF** (`human_face_feat_mbf_s8_v1.espdl`) + `FaceAi::FeatModel` (`MFN_S8_V1` / `MBF_S8_V1`)
- All 28 official P4 `.espdl` weights in `models/espdl/p4/` **and** `src/espdl/<model>/models/p4/`
- MJPEG **Detect** tab lists the full detector set; web examples `32_EthCocoWeb` (all box detectors) plus `33_EthYolo26Web`, `34_EthCatWeb`, `35_EthDogWeb`, `36_EthHandWeb`, `37_EthPoseWeb`, `38_EthSegWeb`, `39_EthGestureWeb`, `40_EthClsWeb`, `41_EthOcrWeb`
- Also: `idf_examples/09_CocoDetect`, Face examples unchanged

## 3.19.0

**Remain-tab closures** vs `esp_video` README (still `camera_fb_t`, not `esp_video_init()`):

- **V4L2 MMAP**: `v4l.mmap(length, offset)` maps `QUERYBUF.m.offset` onto the DMA FB (zero-copy). `DQBUF` with `V4L2_MEMORY_MMAP` returns the pool index. POSIX `mmap()` syscall is still ENOSYS (ESP-IDF VFS).
- **ISP blob CIDs**: `V4L2_CID_USER_ESP_ISP_*` (CCM, gamma, BF, sharpen, demosaic, WB, LSC, AF, AWB, BLC) via `VIDIOC_S/G_EXT_CTRLS` on `/dev/video0` and `/dev/video20`.
- **SPI1 `/dev/video4`**: second SPI host (`cfg.spi.spi_port` ≠ SPI2). Two SPI cameras allowed on different ports.
- **`V4L2_CTRL_CLASS_ESP_CAM_IOCTL`**: first-party `G_CHIP_ID`, `S_STREAM`, `S_TEST_PATTERN`, `S/G_REG`, `S_GAIN` — no `esp_cam_sensor` link.

- **Debug pipeline**: `src/debug/ESP32P4_Debug` + `APP_DEBUG` / NVS `csi_dbg`. Serial `d` / `d=<mask>` / `d=r`. HTTP `GET /debug`. Component `CSI_S` stalls for PPA, CSI capture, JPEG send.

Lasting IDF-only gaps: closed `esp_ipa` binary, POSIX `mmap()` syscall, other SoCs.

## 3.18.0

**V4L2 M2M + ISP meta** (closes most remaining `esp_video` README device gaps):

- `ESP32P4_V4l2M2m` registers `/dev/video10` (JPEG enc), `/dev/video11` (H.264 enc), `/dev/video12` (JPEG dec), `/dev/video20` (ISP stats meta). Wraps existing HW codecs — not `esp_video_init()`.
- M2M USERPTR queues + read/write; H.264 CIDs (`BITRATE`, `GOP`, `I_PERIOD`, `FORCE_KEY_FRAME`); JPEG `CHROMA_SUBSAMPLING`.
- `ESP32P4_Isp::exportV4l2Stats()` fills `V4L2_META_FMT_ESP_ISP_STATS` from in-process IPA/ISP (partial vs closed `esp_ipa` blob).
- Capture V4L2: `VIDIOC_SET_OWNER`, `VIDIOC_G/S_MOTOR_FMT` (DW9714 when present).
- Example `27_V4l2Ctl` updated. Still no real `mmap()` (ESP-IDF VFS limit).

## 3.17.0

**Dual camera at the same time** (still not two CSI):

- Two `ESP32P4_Camera` objects on **different** buses: CSI+DVP, CSI+SPI, CSI+UVC_HOST. Same `capture()` / `release()` path.
- `esp32p4_cam_dual_ok(a, b)` / `esp32p4_cam_dual_why()` — ESP32-P4 has one MIPI CSI host; two of the same bus still fail.
- SCCB I2C is locked per camera (Wire / Wire1) so CSI IPA AE does not stomp a second sensor. LEDC XCLK uses the next free timer.
- Example `28_DualCam`. USB gadget + host still cannot share the PHY.

## 3.16.0

**V4L2 POSIX + v4l2-ctl interop** without replacing `camera_fb_t`:

- `ESP32P4_V4l2` wraps a started `ESP32P4_Camera`. `cam.begin()` / `capture()` stay the sketch path.
- POSIX node: CSI `/dev/video0`, DVP `/dev/video2`, SPI `/dev/video3`, host UVC `/dev/video40`.
- `v4l.ctl("--list-ctrls")`. Example `27_V4l2Ctl`. Not Arduino `ESP_Video` / mmap M2M `/dev/video10–12`.

## 3.5.1

- **OV2710 stream**: `stream_on`/`stream_off` now use Espressif `0x3008` / `0x4202` (not `0x0100`). Fixes `capture timeout` after a successful probe (confirmed on LilyGO T-Display P4).
- **Camera I²C bus**: `esp32p4_cam_config_t::wire` selects `Wire` (default) or `Wire1`. Dual-bus boards (LilyGO T-Display P4 camera on I2C1) can keep `Wire` for other devices.
- **Docs**: CSI-Cameras / wiki now separate **sensor MIPI** (RAW10/RAW8) from **`cfg.pixel_format`** (`RGB565` after ISP). Do not set `ESP32P4_PIXFORMAT_RAW10` for OV2710.

## 3.5.0

### Smart AE (software auto-exposure)

- New `ESP32P4_SmartAe` (`src/cam/ESP32P4_SmartAe.*`): phone-style metering for CSI RGB565.
- Center-weighted luma + highlight protection; exposure-first then gain; IIR smoothing.
- Dynamic gain ceiling for dark/faded scenes; hard clip only when peak saturates (avoids whitening).
- Wired into MJPEG worker + SENSOR UI (`smart_ae`, `smart_ae_ev`).
- APIs: `MjpegServer::enableSmartAe()` / `smartAe()`; EV bias ±4 half-stops.

### Barcode / QR (zxing-cpp)

- Replaced lightweight QR path with vendored [zxing-cpp](https://github.com/zxing-cpp/zxing-cpp) **v2.3.0** under `src/qr/zxing/`.
- Example `22_EthQrWeb`: Ethernet MJPEG + QR tab; **async** half-res gray decode (JPEG worker never blocks on zxing).
- Stream-safe pipeline: in-place overlay, PPA RGB565→GRAY half-res snap, lean live options, Codabar start/stop stripped from payload.
- QR mode letterboxes FOV (`scaleFit` + CSS `contain`) so codes are not cover-cropped.
- **Per-format enable checkboxes** in the QR sidebar; shows decoded **type name**; settings persist to `/qr/settings.txt` (SD/FFat/LittleFS/SPIFFS) + NVS backup.

**Supported symbologies**

| Family | Formats |
| --- | --- |
| Matrix | QR Code, Micro QR, **rMQR**, Aztec, PDF417 |
| 1D | Code 128, Code 39, Code 93, Codabar, EAN-8, EAN-13, UPC-A, UPC-E, ITF |
| GS1 | DataBar, DataBar Expanded, DataBar Limited |

Omitted (niche / heavy): Data Matrix, MaxiCode, DX Film Edge.

## 3.4.1

- **`idf_examples/21_EthFaceWeb`**: Ethernet MJPEG web UI + official ESP-DL
  `human_face_detect` + `human_face_recognition` (enroll / recognize).
- `ESP32P4_FaceAi` wrapper (`idf_src/`), `MjpegServer::enableFaceUi()`.

## 3.4.0

- **Faster CV**: PPA RGB565→GRAY8 / scale, separable blur, reused morph scratch, fast Sobel interior.
- **Half-res detect** (pyramid) for Edge track + luma Blobs — boxes scaled ×2 (esp32-opencv style lean imgproc).
- `ESP32P4_Ppa::cv()` helpers: `rgb565ToGray`, `rgb565ToGrayScale`, `scaleRgb565`, `fillRect565`.

## 3.3.0

- **OpenCV-inspired** `ESP32P4_Cv` (`src/cv/`): gray, blur, threshold, HSV `inRange`, morph, edges, `findBlobs`, draw.
- **ESP-VISION-inspired** `ESP32P4_VisionAi` (`src/vision/`): letterbox RGB565→RGB888, NMS, softmax, det/pose structs.
- Examples `19_CvColorBlobs`, `20_EthCvPreview` (Ethernet MJPEG + CV overlay via `setFrameHook`).
- `MjpegServer::setFrameHook` for live annotate-before-JPEG.
- Inspired by `micropython-opencv` + `esp-vision` (no vendored OpenCV/imlib).

## 3.2.0

- Vendor **WebFileManager** into `src/wfm/` (no separate Arduino library required).
- Example `18_EthH264RecordFiles`: camera UI ↔ file browser navigation.
- `MjpegServer::setFilesBrowserPort` / `WebFileManager::setHomePort` cross-links.

## 3.1.0

- Rename library to **ESP32CSI_Vision** (CSI/vision brand, not SoC-locked).
- Repo target: `thezerohz/ESP32CSI_Vision`.
- Compat: `ESP32P4_Vision.h`, `ESP32P4_Cam.h`, `ESP32P4_CSI_Camera.h`.

## 3.0.0

- Renamed from `ESP32P4_Cam` to `ESP32P4_Vision`.

## 2.2.2

- Dual-port MJPEG so settings stay realtime.

## 2.2.0 – 2.2.1

- MJPEG UI, PPA framesize, OV5647 controls.

## 2.0.0 – 2.1.0

- Multi-FB CSI, JPEG, PPA, DSP, WHO pipeline, ESP-DL face example.
