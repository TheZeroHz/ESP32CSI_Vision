# COCO / YOLO26 / cat / dog / hand detect (vendored ESP-DL)

Ethernet MJPEG + the full `ESP32P4_ObjectDetect` zoo. Same pattern as `21_EthFaceWeb`.

Focused sketches (same UI, one model family): `33_EthYolo26Web`, `34_EthCatWeb`, `35_EthDogWeb`, `36_EthHandWeb`. Pose / seg / gesture / ImageNet / OCR: `37`–`41`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `coco_detect_yolo11n_s8_v1.espdl` | YOLO11n 640 — bus, person, car, … (80 classes) |
| `coco_detect_yolo11n_320_s8_v1.espdl` | YOLO11n 320 — faster |
| `yolo26n_640_s8_p4.espdl` | YOLO26n 640 — COCO-80 |
| `yolo26n_512_s8_p4.espdl` | YOLO26n 512 — faster |
| `pedestrian_detect_pico_s8_v1.espdl` | Pedestrian-only Pico |
| `espdet_pico_224_224_cat.espdl` / `_416_416_cat` | Cat |
| `espdet_pico_224_224_dog.espdl` / `_416_416_dog` | Dog |
| `espdet_pico_224_224_hand.espdl` | Hand |

Paths on the volume: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**Detect** tab: DETECT toggle, model select (all of the above), score %, last labels. Boxes drawn on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/32_EthCocoWeb
arduino-cli upload -p COM8 --fqbn %FQBN% examples/32_EthCocoWeb
```
