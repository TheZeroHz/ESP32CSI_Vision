# YOLO26n COCO detect (vendored ESP-DL)

Ethernet MJPEG + Ultralytics YOLO26n (80 COCO classes). Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `yolo26n_640_s8_p4.espdl` | YOLO26n 640 — default |
| `yolo26n_512_s8_p4.espdl` | YOLO26n 512 — faster |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**YOLO26** tab: DETECT toggle, 640 / 512 select, score %, last labels. Boxes on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/33_EthYolo26Web
```
