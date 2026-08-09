# 21_EthFaceWeb — Face detect + recognize with Ethernet web UI

Live CSI preview over **Ethernet MJPEG** with Espressif official ESP-DL models:

| Component | Role |
| --- | --- |
| [`espressif/human_face_detect`](https://components.espressif.com/components/espressif/human_face_detect) | MSR+MNP / ESPDet Pico |
| [`espressif/human_face_recognition`](https://components.espressif.com/components/espressif/human_face_recognition) | MFN_S8_V1 features + enroll DB |

Arduino IDE **cannot** build this (no ESP-DL in the Arduino-ESP32 SDK). Use **ESP-IDF 5.4 / 5.5**.

## Board

Guition **JC-ESP32P4-M3** (IP101 Ethernet + OV5647 CSI). Serial **COM8** @ 115200.

## Build & flash

```bat
cd idf_examples\21_EthFaceWeb
idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
idf.py -p COM8 build flash monitor
```

Open `http://<board-ip>/` → **Face detect / recognize** panel.

## Web UI

- **Det model** — MSR+MNP (fast), ESPDet Pico 224 / 416  
- **Enroll face** — store current face feature in `/spiffs/face.db`  
- **Clear DB** — wipe enrolled features  
- Stream overlay: cyan = detected, yellow `ID#` = recognized  

## Related

- Terminal-only detect: `idf_examples/08_FaceDetect`  
- Upstream: [ESP-WHO](https://github.com/espressif/esp-who), [ESP-DL](https://github.com/espressif/esp-dl)
