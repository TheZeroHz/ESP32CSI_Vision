# ESP-DL model zoo (vendored)

Components under `src/espdl/` mirror [espressif/esp-dl/models](https://github.com/espressif/esp-dl/tree/master/models).
P4 weights live in [`models/espdl/p4/`](../models/espdl/p4/) — see that README for filenames.

| Folder | Class / API | Notes |
| --- | --- | --- |
| `human_face_detect` | `HumanFaceDetect` | via `ESP32P4_FaceDetect` / `FaceAi` |
| `human_face_recognition` | `HumanFaceFeat` / `Recognizer` | via `ESP32P4_FaceAi` |
| `coco_detect` | `COCODetect` | via `ESP32P4_ObjectDetect` |
| `pedestrian_detect` | `PedestrianDetect` | via `ESP32P4_ObjectDetect` |
| `cat_detect` | `CatDetect` | via `ESP32P4_ObjectDetect` |
| `dog_detect` | `DogDetect` | via `ESP32P4_ObjectDetect` |
| `hand_detect` | `HandDetect` | via `ESP32P4_ObjectDetect` |
| `yolo26` | `YOLO26` | via `ESP32P4_ObjectDetect` |
| `coco_pose` | `COCOPose` | via `ESP32P4_Pose` |
| `coco_seg` | `COCOSeg` | via `ESP32P4_Seg` |
| `hand_gesture_recognition` | `HandGestureRecognizer` | via `ESP32P4_Gesture` |
| `imagenet_cls` | `ImageNetCls` | via `ESP32P4_Cls` |
| `person_reid` | ReID feat | via `ESP32P4_Reid` |
| `pp_ocr_v6` | `PPOCRV6` | via `ESP32P4_Ocr` |
| `speaker_verification` | speaker embed | via `ESP32P4_Speaker`; skipped unless `dl_fbank` vendored |
| `color_detect` | color blobs | no `.espdl`; needs opencv-mobile |
| `motion_detect` | frame diff | no `.espdl` (also `ESP32P4_Dsp`) |
| `feat_database` | feat DB helpers | ReID / verification |

Arduino loads all of these from SD (`espdl_arduino_config.h`). Include native headers via shims in `src/*.hpp` (e.g. `#include "cat_detect.hpp"`).

Web examples: `32_EthCocoWeb` (full box-detect zoo), `33_EthYolo26Web`, `34_EthCatWeb`, `35_EthDogWeb`, `36_EthHandWeb`, `37_EthPoseWeb`, `38_EthSegWeb`, `39_EthGestureWeb`, `40_EthClsWeb`, `41_EthOcrWeb`. Face: `21_EthFaceWeb`.
