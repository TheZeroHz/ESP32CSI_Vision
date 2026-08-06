# 08_FaceDetect — ESP-DL face detection (ESP-IDF + Arduino)

Live CSI frames from Guition JC-ESP32P4-M3 (OV5647) run through Espressif
`human_face_detect` (ESP-DL). Sketch uses Arduino `setup()` / `loop()` via
**Arduino as an ESP-IDF component**.

Arduino IDE **cannot** build this example (no esp-dl in the Arduino-ESP32 3.3.x
prebuilt SDK). Use ESP-IDF 5.4 or 5.5.

## Prerequisites

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/) **v5.4.x or v5.5.x**
2. Open an ESP-IDF terminal (`export.bat` / Espressif IDE)
3. This folder is the project root

## Build & flash (Windows, COM8)

```bat
idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
idf.py -p COM8 build flash monitor
```

Serial 115200. Point the camera at a face; you should see lines like:

```text
faces=1  45ms  800x640
  [0] score=0.91 box=120,80 180x200  eyeL=(...) nose=(...) eyeR=(...)
```

## Models

Default: `MSRMNP_S8_V1` (two-stage, good speed on P4).

Change in `main.cpp`:

```cpp
face.begin(ESP32P4_FaceDetect::ESPDET_PICO_224);
```

Models load from flash rodata (`CONFIG_HUMAN_FACE_DETECT_MODEL_IN_FLASH_RODATA`).

## Related

- Capture / JPEG / MJPEG without IDF: Arduino examples `01`–`07`
- Upstream: [ESP-WHO](https://github.com/espressif/esp-who), [ESP-DL](https://github.com/espressif/esp-dl)
