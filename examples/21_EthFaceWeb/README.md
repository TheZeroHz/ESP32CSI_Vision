# Face detect / recognize (vendored ESP-DL)

## Capture / stream

- Sensor **auto** → OV5647 **1920×1080 RAW10** (falls back to 800×640 if needed)
- MJPEG default **Half** of native (960×540) — pick Native / ~720 / Quarter in the UI
- Stream sizes are **computed from live sensor** `native_w×native_h` (16-aligned)

## Face AI

- Center **1:1 crop** then scale to model size: MSR **320²**, ESPDet **224² / 416²**
- Boxes mapped back onto the stream frame

## Web UI

Detection / Recognition toggles, named enroll → SD `face.db` + `face_names.txt`, delete/clear.

## SD models

`human_face_detect_msr/mnp_*.espdl` + `human_face_feat_mfn_s8_v1.espdl` under `/models/p4/`.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/21_EthFaceWeb
arduino-cli upload -p COM8 --fqbn %FQBN% examples/21_EthFaceWeb
```
