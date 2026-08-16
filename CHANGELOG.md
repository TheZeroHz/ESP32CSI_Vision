# Changelog

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
