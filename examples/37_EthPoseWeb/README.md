# COCO pose (vendored ESP-DL)

Ethernet MJPEG + YOLO11n-Pose (COCO-17 keypoints). Same Detect-tab pattern as `32_EthCocoWeb`.

## Models (copy to preferred storage)

From library `models/espdl/p4/`:

| File | Use |
| --- | --- |
| `coco_pose_yolo11n_pose_s8_v2.espdl` | Pose v2 — default |
| `coco_pose_yolo11n_pose_s8_v1.espdl` | Pose v1 |

Paths: `/models/p4/*.espdl`. Upload via WebFileManager `:82` if missing.

## Web UI

**Pose** tab: DETECT toggle, v2 / v1 select, score %. Skeletons drawn on the MJPEG stream.

## Build

```bat
set FQBN=esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn %FQBN% examples/37_EthPoseWeb
```
