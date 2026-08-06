# ESP32CSI_Vision

**Author:** [Rakib Hasan](https://github.com/thezerohz) ([@thezerohz](https://github.com/thezerohz))

**ESP32 MIPI CSI computer-vision library** for Arduino IDE and ESP-IDF — camera capture, PSRAM frames, hardware JPEG, PPA, motion DSP, WiFi MJPEG UI, WHO-style pipelines, and ESP-DL face detection.

Named for **CSI vision**, not a single chip: **ESP32-P4 today**, ready for other CSI-capable Espressif SoCs later.

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Arduino-ESP32](https://img.shields.io/badge/Arduino--ESP32-3.3.x-blue)](https://github.com/espressif/arduino-esp32)
[![MIPI CSI](https://img.shields.io/badge/Interface-MIPI%20CSI-orange)](https://www.espressif.com/)
[![GitHub](https://img.shields.io/badge/GitHub-thezerohz%2FESP32CSI__Vision-black)](https://github.com/thezerohz/ESP32CSI_Vision)

> Compat: `#include <ESP32P4_Vision.h>` / `<ESP32P4_Cam.h>` still work.

---

## Why this name

| Name | Problem |
| --- | --- |
| `ESP32P4_Cam` / `ESP32P4_Vision` | Locked to one SoC |
| **`ESP32CSI_Vision`** | Describes the **interface + domain** (CSI + vision). New chips with MIPI CSI can join the same brand. |

---

## Features

| Capability | Arduino IDE | ESP-IDF 5.4 / 5.5 |
| --- | :---: | :---: |
| OV5647 / IMX708 MIPI CSI + ISP → RGB565 | Yes (P4) | Yes (P4) |
| PSRAM multi-framebuffer capture | Yes | Yes |
| Hardware JPEG encode / decode | Yes | Yes |
| PPA scale / rotate / mirror | Yes | Yes |
| Motion DSP + MJPEG settings UI | Yes | Yes |
| Who-style async frame pipeline | Yes | Yes |
| ESP-DL human face detect | — | Yes (`idf_examples/08_FaceDetect`) |

---

## Install

```text
https://github.com/thezerohz/ESP32CSI_Vision
```

```cpp
#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;  // capture API (P4 CSI backend today)
cam.begin(ESP32P4_BOARD_GUITION_M3);
```

```ini
lib_deps = https://github.com/thezerohz/ESP32CSI_Vision.git
```

---

## HTTP API

| Endpoint | Port | Role |
| --- | --- | --- |
| `/` `/control` `/status` `/jpg` `/capture` | **80** | UI + live settings |
| `/stream` | **81** | MJPEG (does not block settings) |

---

## Examples

`01_CamTest` … `07_WhoPipeline` (Arduino) · `idf_examples/08_FaceDetect` (ESP-IDF)

```text
python examples/04_WiFiMjpeg/cam_wifi_viewer.py 192.168.0.3 81
```

---

## GitHub repo metadata (copy/paste)

**Repository name:** `ESP32CSI_Vision`

**About / description:**

```text
ESP32 MIPI CSI computer-vision library for Arduino & ESP-IDF: OV5647/IMX708, PSRAM frames, HW JPEG, PPA/DSP, WiFi MJPEG UI, ESP-DL face detect. Named for CSI vision — not locked to one SoC (ESP32-P4 today).
```

**Topics:**

```text
esp32
mipi-csi
csi
csi-camera
esp32-p4
esp32p4
ov5647
imx708
computer-vision
embedded-vision
arduino
arduino-library
jpeg
mjpeg
psram
isp
ppa
face-detection
esp-dl
esp-who
webcam
iot-camera
guition
```

**SEO keywords:**  
`esp32, mipi-csi, csi, csi-camera, esp32-p4, ov5647, imx708, computer-vision, vision, jpeg, mjpeg, psram, isp, ppa, face-detection, esp-dl, esp-who, arduino, webcam, embedded-vision`

---

## Author

- **Name:** Rakib Hasan  
- **GitHub:** [@thezerohz](https://github.com/thezerohz)  
- **Repo:** [https://github.com/thezerohz/ESP32CSI_Vision](https://github.com/thezerohz/ESP32CSI_Vision)

## License

MIT — Copyright (c) 2026 Rakib Hasan.
