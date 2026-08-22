# COCO segmentation (vendored ESP-DL)

Ethernet MJPEG + YOLO11n-Seg instance masks. Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `coco_seg_yolo11n_seg_s8_v1.espdl` | YOLO11n-Seg |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**Seg** tab: DETECT toggle, score %. Masks + boxes on the MJPEG stream.

Inference runs on a side task (~3s). The live preview keeps encoding; turn DETECT off if you only want the camera.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/38_EthSegWeb
```
