# Changelog

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
