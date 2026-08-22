# ESP-DL model weights (ESP32-P4)

**28 / 28** official P4 `.espdl` files from [espressif/esp-dl/models](https://github.com/espressif/esp-dl/tree/master/models).  
The same files also sit next to each component under `src/espdl/<name>/models/p4/` (IDF flash pack).

Copy onto the board at:

```text
{mount}/models/p4/*.espdl
```

Examples: `/sdcard/models/p4/…` or `/ffat/models/p4/…`  
Upload via WebFileManager (`:82`) if the card is empty.

## Object detect (`ESP32P4_ObjectDetect`)

| File | UI / enum |
| --- | --- |
| `coco_detect_yolo11n_s8_v1.espdl` | `COCO_YOLO11N` |
| `coco_detect_yolo11n_320_s8_v1.espdl` | `COCO_YOLO11N_320` |
| `pedestrian_detect_pico_s8_v1.espdl` | `PEDESTRIAN_PICO` |
| `espdet_pico_224_224_cat.espdl` | `CAT_224` |
| `espdet_pico_416_416_cat.espdl` | `CAT_416` |
| `espdet_pico_224_224_dog.espdl` | `DOG_224` |
| `espdet_pico_416_416_dog.espdl` | `DOG_416` |
| `espdet_pico_224_224_hand.espdl` | `HAND_224` |
| `yolo26n_640_s8_p4.espdl` | `YOLO26_640` |
| `yolo26n_512_s8_p4.espdl` | `YOLO26_512` |

## Face (`ESP32P4_FaceAi` / `FaceDetect`)

| File | Use |
| --- | --- |
| `human_face_detect_msr_s8_v1.espdl` | MSR detect |
| `human_face_detect_mnp_s8_v1.espdl` | MNP landmarks |
| `espdet_pico_224_224_face.espdl` | ESPDet 224 |
| `espdet_pico_416_416_face.espdl` | ESPDet 416 |
| `human_face_feat_mfn_s8_v1.espdl` | Recognize / enroll (`FeatModel::MFN_S8_V1`) |
| `human_face_feat_mbf_s8_v1.espdl` | Higher-accuracy feat (`FeatModel::MBF_S8_V1`) |

## Pose / seg / cls / gesture / ReID / OCR / speaker

| File | Arduino class |
| --- | --- |
| `coco_pose_yolo11n_pose_s8_v1.espdl` / `_v2` | `ESP32P4_Pose` |
| `coco_seg_yolo11n_seg_s8_v1.espdl` | `ESP32P4_Seg` |
| `imagenet_cls_mobilenetv2_s8_v1.espdl` | `ESP32P4_Cls` |
| `mobilenetv2_0_5_128_128_gesture.espdl` | `ESP32P4_Gesture` (+ hand detect) |
| `person_reid_feat_osn_s8_v1.espdl` | `ESP32P4_Reid` (+ pedestrian) |
| `pp_ocr_v6_det_s8.espdl` + `pp_ocr_v6_rec_*.espdl` | `ESP32P4_Ocr` |
| `sv_tdnn_tiny_3s.espdl` / `_6s` | `ESP32P4_Speaker` |

No weights (classical): `color_detect`, `motion_detect`. Helper: `feat_database`.

**Optional compile:** `color_detect` needs opencv-mobile; `speaker_verification` needs esp-dl audio (`dl_fbank` / `dl_audio_wav`). Both are skipped automatically unless those deps are present (or you define `ESP32P4_ESPDL_ENABLE_COLOR_DETECT` / `ESP32P4_ESPDL_ENABLE_SPEAKER`).

## Examples

- Detect zoo web UI: `examples/32_EthCocoWeb`
- YOLO26 / cat / dog / hand: `33_EthYolo26Web`, `34_EthCatWeb`, `35_EthDogWeb`, `36_EthHandWeb`
- Pose / seg / gesture / ImageNet / OCR: `37_EthPoseWeb` … `41_EthOcrWeb`
- Face web UI: `examples/21_EthFaceWeb`
- Serial COCO: `idf_examples/09_CocoDetect`
- Serial face: `idf_examples/08_FaceDetect`
