# Cat detect (vendored ESP-DL)

Ethernet MJPEG + ESPDet Pico cat. Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `espdet_pico_224_224_cat.espdl` | Pico 224 — default / faster |
| `espdet_pico_416_416_cat.espdl` | Pico 416 — more accurate |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**Cat** tab: DETECT toggle, 224 / 416 select, score %. Boxes on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/34_EthCatWeb
```
