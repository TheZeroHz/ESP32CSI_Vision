# PaddleOCR v6 (vendored ESP-DL)

Ethernet MJPEG + PP-OCR v6 detect + recognize. Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `pp_ocr_v6_det_s8.espdl` | Text detect (required) |
| `pp_ocr_v6_rec_s16.espdl` | Recognize s16 — default |
| `pp_ocr_v6_rec_s8.espdl` | Recognize s8 — faster |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**OCR** tab: DETECT toggle, rec s16 / s8, score %. Quads + text on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/41_EthOcrWeb
```
