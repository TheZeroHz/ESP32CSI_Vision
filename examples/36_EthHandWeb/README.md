# Hand detect (vendored ESP-DL)

Ethernet MJPEG + ESPDet Pico hand. Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `espdet_pico_224_224_hand.espdl` | Pico 224 hand |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

For 8-class gestures on the same boxes, use `39_EthGestureWeb`.

## Web UI

**Hand** tab: DETECT toggle, score %. Boxes on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/36_EthHandWeb
```
