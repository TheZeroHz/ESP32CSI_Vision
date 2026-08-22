# Hand gesture (vendored ESP-DL)

Ethernet MJPEG + Pico hand detect + MobileNetV2 8-class gestures. Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `espdet_pico_224_224_hand.espdl` | Hand boxes |
| `mobilenetv2_0_5_128_128_gesture.espdl` | Gesture labels |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**Gesture** tab: DETECT toggle, score %. Hand boxes + gesture names on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/39_EthGestureWeb
```
