# ImageNet classify (vendored ESP-DL)

Ethernet MJPEG + MobileNetV2 ImageNet-1000. Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `imagenet_cls_mobilenetv2_s8_v1.espdl` | ImageNet MobileNetV2 |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**Cls** tab: DETECT toggle, score % (filters top-5). Labels overlaid on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/40_EthClsWeb
```
