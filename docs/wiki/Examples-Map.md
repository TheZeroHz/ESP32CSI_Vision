# Examples Map

All Arduino sketches: **File → Examples → ESP32CSI_Vision → …**

| # | Sketch | APIs used | What you learn |
| --- | --- | --- | --- |
| 01 | `01_CamTest` | `ESP32P4_Camera`, PSRAM free | Capture loop, release FB |
| 02 | `02_JpegSnapshot` | `Camera` + `Jpeg` + `psram_alloc` | HW JPEG encode |
| 03 | `03_JpegDecode` | `Jpeg::decodeInfo` / `decode` | Decode without camera |
| 04 | `04_WiFiMjpeg` | `MjpegServer`, WiFi pins | Full UI + dual-port stream |
| 05 | `05_MotionDetect` | `Dsp::detect` | Motion ROI |
| 06 | `06_PpaScale` | `Ppa::scale`, `Img::downsample2x565` | HW scale + CPU fallback |
| 07 | `07_WhoPipeline` | `WhoPipeline` | Callback + `waitFrame` |
| 08 | `idf_examples/08_FaceDetect` | `FaceDetect` | ESP-DL faces (**ESP-IDF**) |
| 21 | `idf_examples/21_EthFaceWeb` | `FaceAi` + MJPEG | Ethernet web UI + detect/recognize (**ESP-IDF**) |
| 09 | `09_SdCard` | `ESP32P4_Sd` | microSD R/W |
| 14 | `14_EthSdBrowser` | Bundled WebFileManager + Sd | Ethernet file explorer |
| 15 | `15_MicSdRecord` | ES8311 + I2S + Sd | Mic → WAV on SD (Guition M3) |
| 16 | `16_MicSdWebFileManager` | Mic + bundled WebFileManager | GPIO1 (or Serial `r`) records 10s into `/Recording` + web UI |
| 17 | `17_EthH264Record` | `MjpegServer`, ETH/IP101, H264, Mic, Sd | Ethernet MJPEG + Capture Img + H.264/MP4 + mic |
| 18 | `18_EthH264RecordFiles` | 17 + bundled WebFileManager | Camera UI ↔ SD file browser (ports 80 ↔ 82) |
| 19 | `19_CvColorBlobs` | `ESP32P4_Cv`, `ESP32P4_VisionAi` | HSV color blobs + letterbox tensor for AI |
| 20 | `20_EthCvPreview` | Eth + `MjpegServer` + `ESP32P4_Cv` | Live Ethernet preview with CV blob overlays |

---

## 01 — CamTest

```cpp
#include <ESP32CSI_Vision.h>
ESP32P4_Camera cam;

void setup() {
  Serial.begin(115200);
  cam.begin(ESP32P4_BOARD_GUITION_M3);
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) return;
  Serial.printf("%ux%u\n", fb->width, fb->height);
  cam.release(fb);
}
```

**Prefs:** board preset only.

---

## 02 — JpegSnapshot

```cpp
ESP32P4_Jpeg jpeg;
uint8_t *jpg = (uint8_t *)esp32p4_psram_alloc(200 * 1024);
jpeg.begin(cam.width(), cam.height(), 45);  // quality preference
size_t n = jpeg.encode(fb, jpg, 200 * 1024);
```

**Prefs:** JPEG `quality` in `begin` / `setQuality`.

---

## 03 — JpegDecode

Smoke-tests decoder with embedded tiny JPEG — no CSI required.

**Prefs:** `jpeg.begin(max_w, max_h, quality)`.

---

## 04 — WiFiMjpeg

**Prefs (edit sketch):**

| Preference | Where |
| --- | --- |
| Wi‑Fi SSID/pass | `WIFI_SSID` / `WIFI_PASS` |
| C6 SDIO pins | `WiFi.setPins(...)` |
| JPEG quality | `stream.begin(&cam, 80, 35)` |
| Live knobs | UI or `/control` — see [HTTP & Preferences](HTTP-and-Preferences.md) |

```cpp
stream.begin(&cam, 80, 35);
// UI :80  stream :81
```

Viewer:

```bash
python examples/04_WiFiMjpeg/cam_wifi_viewer.py <ip> 81
```

---

## 17 — EthMjpeg

Same product path as **04**, but brings up Guition M3 **IP101 Ethernet** instead of C6 Wi‑Fi, plus SD **Capture Img** → `/IMG`.

**Prefs (edit sketch):**

| Preference | Where |
| --- | --- |
| ETH PHY pins | `ETH_PHY_*` / `ETH.begin(...)` |
| JPEG quality | `stream.begin(&cam, 80, 35)` |
| Live knobs | UI or `/control` — see [HTTP & Preferences](HTTP-and-Preferences.md) |

```cpp
ETH.begin(...);
stream.begin(&cam, 80, 35);
// UI :80  stream :81
```

Viewer (reuse Wi‑Fi script):

```bash
python examples/04_WiFiMjpeg/cam_wifi_viewer.py <ip> 81
```

---

## 05 — MotionDetect

```cpp
dsp.begin(cam.width(), cam.height(), 22);  // threshold preference
esp32p4_motion_t m{};
dsp.detect(fb, &m);
```

**Prefs:** `threshold` — lower = more sensitive.

---

## 06 — PpaScale

```cpp
ppa.scale(fb, scaled, cap, 400, 320);  // dst_w, dst_h preferences
```

Fallback: `ESP32P4_Img::downsample2x565`.

---

## 07 — WhoPipeline

```cpp
who.onFrame(on_frame);
who.begin(&cam, 2);           // queue_len preference
who.waitFrame(&fb, 2000);     // timeout preference
```

---

## 08 — FaceDetect (IDF)

```cpp
face.begin(ESP32P4_FaceDetect::MSRMNP_S8_V1);  // model preference
int n = face.detect(fb, faces, 8);             // max_out preference
```

Not buildable in Arduino IDE alone.

---

## Suggested learning order

1. `01` → confirm CSI + PSRAM  
2. `02` → JPEG  
3. `04` → Wi‑Fi UI (main product path)  
4. `05` / `06` / `07` → DSP, PPA, pipeline  
5. `08` → faces on ESP-IDF  

← [Home](Home.md) · [API Reference](API-Reference.md) →
