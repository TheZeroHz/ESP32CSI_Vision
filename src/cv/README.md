# Classic CV + Vision AI helpers

Lean RGB565/GRAY8 imgproc (micropython-opencv / [esp32-opencv](https://github.com/joachimBurket/esp32-opencv)
ideas: small footprint, PSRAM buffers, dst reuse) — **no full OpenCV**.

ESP32-P4 **PPA** accelerates RGB565→GRAY8, scale, and fill. Detection runs at
½ resolution (pyramid) then boxes are scaled back — typically ~3–5× faster.

| Module | Header | Role |
| --- | --- | --- |
| `ESP32P4_Cv` | `src/cv/ESP32P4_Cv.h` | gray (PPA), blur, threshold, HSV, morph, edges, findBlobs |
| `ESP32P4_Ppa` | `src/ppa/ESP32P4_Ppa.h` | HW SRM / fill / rgb565ToGrayScale |
| `ESP32P4_VisionAi` | `src/vision/ESP32P4_VisionAi.h` | letterbox, NMS, det/pose structs |
| `ESP32P4_FaceDetect` | `idf_src/` | ESP-DL faces (ESP-IDF only) |

Examples: `19_CvColorBlobs`, `20_EthCvPreview` — modes include **Edge track**.
