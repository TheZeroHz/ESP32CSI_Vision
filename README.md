# ESP32CSI_Vision

Arduino library for **ESP32-P4 MIPI CSI** cameras.

## CSI sensors

First-party MIPI drivers (no `ESP_Video`). `cam.begin(board)` **AUTO-probes** the registry. Full detail: [docs/CSI-Cameras.md](docs/CSI-Cameras.md).

| Status | Sensors |
| --- | --- |
| **Supported** (RGB565 capture) | **SC2336**, **OV5647**, **OV9281**, **SC202CS**, **SC1346**, **SC035HGS**, **IMX708** |
| **Experimental** (wired, validate on hardware) | **OV5645**, **OV2710**, **SC030IOT**, **OS02N10**, **OS04C10**, **STI2250**, **MIRA220**, **IMX219** |
| **Detect** (chip-ID only for now) | **GC2145**, **SC121AT**, **IMX477**, **GC2083**, **GC2093**, **IMX335**, **IMX415**, **OV7251**, **IMX296**, **IMX462**, **Arducam-IMX500** |

Common boards: Guition M3 / Waveshare Nano → OV5647 or IMX708 · Espressif Function-EV → SC2336.

**Includes:**

- Live camera capture (MIPI CSI → RGB565 in PSRAM)
- Hardware JPEG encode / decode
- MJPEG web stream + settings UI (`:80` / `:81`)
- Photo capture to SD **or** flash (`/IMG` on SD / FFat / LittleFS / SPIFFS)
- H.264 video recording to preferred storage (`/VIDEO` MP4)
- Onboard mic (ES8311) waveform + audio in recordings
- OpenCV-like CV (gray, blur, threshold, morph, HSV blobs, edges, draw)
- OpenCV-style core types (`Mat`, `Point`, `Rect`, ROI, PSRAM buffers)
- ESP-DL face detect / enroll / recognize (`.espdl` on preferred storage)
- Web file manager with multi-volume (SD + FFat + LittleFS, …)
- Ethernet or Wi‑Fi networking (board-dependent)
- PPA hardware scale / rotate / mirror / color convert
- Motion DSP helpers

```cpp
#include <ESP32CSI_Vision.h>
```

**Target:** ESP32-P4 + PSRAM · **Arduino-ESP32 3.3.x** · Tested board: Guition JC-ESP32P4-M3 (OV5647 / IMX708)

Compared with Arduino `ESP_Video`: you get a ready RGB565 `camera_fb_t` path that MJPEG / face / H.264 / CV already share, instead of wiring V4L2 device nodes yourself.

---

## Install

Copy or clone into:

```text
Documents/Arduino/libraries/ESP32CSI_Vision
```

Board settings (typical):

| Setting | Value |
| --- | --- |
| Board | ESP32P4 Dev Module |
| PSRAM | **Enabled** |
| Flash Size | 16MB (if applicable) |
| Partition | `app3M_fat9M_16MB` (or similar large app) |

FQBN example:

```text
esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
```

---

## Modules (what you get)

| Module | Class / API | Purpose |
| --- | --- | --- |
| Camera | `ESP32P4_Camera` | MIPI CSI → RGB565 in PSRAM |
| JPEG | `ESP32P4_Jpeg` | HW encode / decode |
| PPA | `ESP32P4_Ppa` | HW scale / rotate90 / mirror / RGB565→gray |
| Stream | `ESP32P4_MjpegServer` | Web UI `:80` + MJPEG `:81` |
| SD | `ESP32P4_Sd` | microSD mount / files |
| Storage | `ESP32P4_StoragePref` | Prefer SD / FFat / LittleFS / SPIFFS / AUTO |
| Capture | `enableCapture()` / `enableSdCapture()` | JPEG stills → `/IMG` on any `fs::FS` |
| Record | `enableVideoRecord()` | H.264 MP4 → `/VIDEO` on any `fs::FS` |
| Mic | `ESP32P4_Mic` | ES8311 levels + PCM into MP4 |
| CV | `ESP32P4_Cv` | Gray, blur, thresh, morph, HSV blobs, edges, draw |
| OpenCV core | `esp_cv::Mat` / `cv::Mat` | Mat, Point, Rect, Scalar, ROI, PSRAM |
| Face | `ESP32P4_FaceAi` | Detect + enroll + recognize (`.espdl` on storage) |
| Files | `WebFileManager` | Multi-volume browser (usually `:82`) |
| DSP | `ESP32P4_Dsp` | Motion / frame difference |
| Mem | `esp32p4_psram_*` | Aligned PSRAM alloc |

---

## Quick start

### Capture a frame

```cpp
#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;

void setup() {
  Serial.begin(115200);
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    while (true) delay(1000);
  }
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);  // timeout ms
  if (!fb) return;
  // fb->buf = RGB565, fb->len = w*h*2
  cam.release(fb);                      // always release
}
```

### MJPEG web UI

```cpp
ESP32P4_Camera cam;
ESP32P4_MjpegServer stream;

void setup() {
  cam.begin(ESP32P4_BOARD_GUITION_M3);
  // WiFi / Ethernet bring-up here…
  stream.begin(&cam, 80, 35);   // UI :80, stream :81, JPEG quality 35
}

void loop() {
  stream.loop();
}
```

Open `http://<ip>/` (settings) and `http://<ip>:81/stream` (MJPEG).

### Stills + video record (storage preference)

```cpp
ESP32P4_Sd sd;
ESP32P4_StoragePref store;
ESP32P4_H264 h264;
ESP32P4_Mic mic;

// AUTO = SD → FFat → LittleFS → SPIFFS
// Or force: ESP32P4_STORAGE_FFAT / ESP32P4_STORAGE_SD / …
store.begin(ESP32P4_STORAGE_AUTO, false, &sd);

h264.begin(640, 480, 30, 1500000);
mic.begin(16000);

stream.begin(&cam, 80, 35);
stream.enableCapture(&store.fs(), "/IMG");              // any mounted FS
stream.enableVideoRecord(&store.fs(), &h264, "/VIDEO"); // Record / Stop
if (mic.ready()) stream.enableMic(&mic);
```

`enableSdCapture(&sd, …)` / `enableVideoRecord(&sd, …)` still work (SD shorthand).

### Face detect / enroll (Arduino)

Models on the preferred volume (Arduino paths are root-relative):

```text
/models/p4/human_face_detect_msr_s8_v1.espdl
/models/p4/human_face_detect_mnp_s8_v1.espdl
/models/p4/human_face_feat_mfn_s8_v1.espdl      // enroll / recognize
```

ESP-DL / `fopen` use the VFS root (`/sdcard`, `/ffat`, `/littlefs`, …).  
`ESP32P4_StoragePref::begin()` sets this automatically.

```cpp
ESP32P4_FaceAi face;
char db[64], names[64];
store.vfsPath(db, sizeof(db), "/face/face.db");
store.vfsPath(names, sizeof(names), "/face/names.txt");
face.begin(ESP32P4_FaceDetect::MSRMNP_S8_V1, db, names);

esp32p4_face_id_t out[8];
int n = face.run(rgb565, w, h, out, 8, /*recognize=*/true);
ESP32P4_FaceAi::draw(rgb565, w, h, out, n);

face.requestEnroll("Alice");   // N samples → one feature
face.cancelEnroll();
face.deleteName("Alice");
face.setThresh(0.5f);
```

See example `21_EthFaceWeb` (`#define APP_STORAGE …`).

---

## API reference

All public APIs from `#include <ESP32CSI_Vision.h>`.  
Deep `src/espdl/` internals are not listed — use Face classes only.

### Camera — `ESP32P4_Camera`

| API | Meaning |
| --- | --- |
| `esp32p4_cam_config_default()` | Default pin/format config |
| `esp32p4_cam_config_board(board)` | Board preset config |
| `begin(board)` / `begin(cfg)` | Init CSI + ISP + PSRAM FBs |
| `end()` | Tear down |
| `capture(timeout_ms)` | Next frame (`camera_fb_t*`) or `nullptr` |
| `release(fb)` | Return FB to pool |
| `setTestPattern` / `testPattern` | Sensor color bars |
| `setHMirror` / `setVFlip` | Geometry (+ ISP Bayer sync) |
| `setAEC` / `setAGC` | Auto exposure / gain |
| `setExposure` / `setGain` / `setGainCeiling` | Manual exposure path |
| `getHMirror` / `getVFlip` / `getAEC` / `getAGC` / `getExposure` / `getGain` / `getGainCeiling` | Read back |
| `width` / `height` | Active size |
| `sensorAddress` / `detected` / `sensorType` / `sensorName` | Sensor info |
| `newTransCount` / `doneCount` | CSI transfer stats |
| `fbCount` / `psramOk` | FB pool / PSRAM health |

**Enums:** `esp32p4_board_t` (`GUITION_M3`, `WAVESHARE_NANO`, `FUNCTION_EV`, `CUSTOM`) · `esp32p4_cam_sensor_t` (`AUTO` + Tier A/B/C IDs — see [docs/CSI-Cameras.md](docs/CSI-Cameras.md)) · `esp32p4_cam_framesize_t` · `esp32p4_cam_pixformat_t` (`RGB565`, `RAW10`, `RAW8`)

**`camera_fb_t`:** `buf`, `len`, `width`, `height`, `format`, `timestamp_us`

### CSI camera matrix (summary)

| Status | Sensors |
| --- | --- |
| Supported | SC2336, OV5647, OV9281, SC202CS, SC1346, SC035HGS, IMX708 |
| Experimental | OV5645, OV2710, SC030IOT, OS02N10, OS04C10, STI2250, MIRA220, IMX219 |
| Detect | GC2145, SC121AT, IMX477, GC2083/2093, IMX335/415, OV7251, IMX296/462, Arducam-IMX500 |

`cam.begin(board)` probes `ESP32P4_SENSOR_AUTO` unless you set `cfg.sensor`.

---

### JPEG — `ESP32P4_Jpeg`

| API | Meaning |
| --- | --- |
| `begin(max_w, max_h, quality)` | Init HW JPEG |
| `end()` | Free codec buffers |
| `encode(fb, out, cap)` / `encode(rgb565, w, h, out, cap)` | RGB565 → JPEG bytes |
| `decodeInfo(jpg, len, &w, &h)` | Read JPEG size |
| `decode(jpg, len, rgb_out, …)` | JPEG → RGB |
| `setQuality(q)` | Quality 1–100 |
| `clearInput()` | Clear encoder input |

---

### PPA — `ESP32P4_Ppa`

| API | Meaning |
| --- | --- |
| `begin` / `end` | Open/close PPA |
| `scale(fb, dst, …)` | Stretch scale |
| `scaleRgb565` / `scaleFit` / `scaleCover` | RGB565 resize (stretch / letterbox / cover-crop) |
| `rotate90` | 90° rotate |
| `mirror(…, mx, my)` | H/V mirror |
| `rgb565ToGray` / `rgb565ToGrayScale` | HW gray (+ optional scale) |
| `fillRect565` | HW solid fill |
| `ESP32P4_Ppa::cv()` | Shared singleton for CV (separate from stream) |

---

### MJPEG / web UI — `ESP32P4_MjpegServer`

| API | Meaning |
| --- | --- |
| `begin(cam, port, quality)` | UI on `port`, MJPEG on `port+1` |
| `loop` / `end` | Pump HTTP / shutdown |
| `setQuality` / `setFrameSkip` / `setFramesize` | Live stream knobs |
| `quality` / `frameSkip` / `framesize` / `outWidth` / `outHeight` | Read stream state |
| `controlPort` / `streamPort` | Bound ports |
| `sent` / `lastJpegBytes` | Stats |
| `enableCapture(fs, folder)` / `enableSdCapture(sd\|fs, folder)` | Stills → storage (`/IMG`) |
| `sdCaptureEnabled` / `savedCount` / `lastSavedPath` / `sdFolder` | Capture status |
| `enableVideoRecord(fs\|sd, h264, folder)` / `disableVideoRecord` | Record → MP4 (`/VIDEO`) |
| `videoRecordEnabled` / `isRecording` / `videosSaved` / `lastVideoPath` / `videoFolder` | Record status |
| `enableMic` / `disableMic` / `micEnabled` | Mic waveform + PCM in MP4 |
| `setFilesBrowserPort` / `filesBrowserPort` | Link to WebFileManager |
| `setFrameHook(fn, user)` / `clearFrameHook` | Annotate RGB565 before JPEG (not H.264 FB) |
| `enableCvDashboard` / `cvDashboardEnabled` / `cvConfig` | Built-in CV UI modes |
| `enableFaceUi` / `faceUiEnabled` / `faceUi()` | Face panel shared state |
| `faceResLocked` / `syncFaceStreamSize` | Face forces stream size |

**`FaceUi` fields:** `detect_en`, `recog_en`, `model`, `faces`, `ms`, `feats`, `thr_pct`, `enroll_*`, `roster`, `db_path`, request flags (`enroll_req`, `clear_req`, `delete_req`, `thr_req`, `settings_dirty`, …)

**Stream framesize**

| Value | Enum | Size |
| :---: | --- | --- |
| 0 | `ESP32P4_STREAM_FHD` | 1920×1072 |
| 1 | `ESP32P4_STREAM_HD` | 1280×720 |
| 2 | `ESP32P4_STREAM_XGA` | 1024×576 |
| 3 | `ESP32P4_STREAM_SVGA` | 800×640 |
| 4 | `ESP32P4_STREAM_VGA` | 640×480 |
| 5 | `ESP32P4_STREAM_HVGA` | 480×320 |
| 6 | `ESP32P4_STREAM_CIF` | 400×288 |
| 7 | `ESP32P4_STREAM_QVGA` | 320×240 |
| 8 | `ESP32P4_STREAM_HQVGA` | 240×176 |
| 9 | `ESP32P4_STREAM_QQVGA` | 160×128 |

---

### HTTP (camera UI)

| Endpoint | Port | Role |
| --- | :---: | --- |
| `GET /` | 80 | Settings UI |
| `GET /control?var=&val=` | 80 | Change setting |
| `GET /status` | 80 | JSON status |
| `GET /capture` / `/jpg` | 80 | One JPEG in browser |
| `GET /capture_img` | 80 | Save JPEG to storage |
| `GET /stream` | 81 | Multipart MJPEG |

| `var` | `val` | Meaning |
| --- | --- | --- |
| `quality` | 4–63 | JPEG quality |
| `framesize` | 0–9 | Stream size |
| `frameskip` | 0–8 | Skip frames |
| `hmirror` / `vflip` | 0/1 | Mirror / flip |
| `aec` / `agc` | 0/1 | Auto exposure / gain |
| `aec_value` | 4–980 | Manual exposure |
| `agc_gain` | 0–1023 | Manual gain |
| `gainceiling` | 16–1023 | Gain ceiling |
| `colorbar` | 0/1 | Test pattern |
| `mic_gain` | 0–100 | Mic gain (if enabled) |
| `face_detect` / `face_recog` | 0/1 | Face panel |
| `face_thr` | 10–95 | Match threshold % |
| `face_enroll` | name | Start enroll |
| `face_model` | 0/1/2 | MSR+MNP / ESPDet224 / ESPDet416 |

---

### SD — `ESP32P4_Sd`

| API | Meaning |
| --- | --- |
| `esp32p4_sd_config_default()` / `esp32p4_sd_config_board(board)` | Config helpers |
| `begin(board\|cfg)` / `end` / `mounted` | Mount / unmount |
| `fs()` | `fs::FS` (`SD_MMC`) |
| `cardType` / `cardSize` / `totalBytes` / `usedBytes` | Card info |
| `writeFile` / `writeBytes` / `appendFile` / `readFile` | Simple I/O |
| `listDir` / `exists` / `remove` / `mkdir` / `rename` | FS ops |

---

### Storage preference — `ESP32P4_StoragePref`

Pick where photos, video, models, face DB, and settings live.

| Kind | Meaning |
| --- | --- |
| `ESP32P4_STORAGE_AUTO` | Try SD → FFat → LittleFS → SPIFFS |
| `ESP32P4_STORAGE_SD` | microSD only |
| `ESP32P4_STORAGE_FFAT` | Flash FAT (`/ffat`) — needs a FAT data partition |
| `ESP32P4_STORAGE_LITTLEFS` | Flash LittleFS (`/littlefs`) |
| `ESP32P4_STORAGE_SPIFFS` | Flash SPIFFS (`/spiffs`) |

| API | Meaning |
| --- | --- |
| `begin(pref, format_flash?, sd*, board)` | Mount preferred volume; sets ESP-DL model mount |
| `kind` / `label` / `vfsRoot` / `fs` / `sd` | Active volume |
| `vfsPath(out, cap, rel)` | Build fopen path (`/ffat` + `/face/db`) |
| `exists` / `mkdir` / `remove` / `writeFile` / `writeBytes` | FS helpers |
| `totalBytes` / `usedBytes` | Capacity |
| `esp32p4_set_model_mount_point` / `esp32p4_model_mount_point` | Low-level model VFS root |

**Tips:** Use partition `app3M_fat9M_16MB` (or similar) for FFat. Prefer SD for long H.264 clips (flash wear + size). Examples use `#define APP_STORAGE …` at the top of the sketch.

---

### H.264 — `ESP32P4_H264`

| API | Meaning |
| --- | --- |
| `esp32p4_h264_cfg_default(w, h)` | Default encode config |
| `begin(w, h, fps, bitrate)` / `begin(cfg)` / `end` / `ready` | Init encoder |
| `width` / `height` / `fps` / `bitrate` / `framesEncoded` | Status |
| `encode(fb\|rgb565, out, …)` | One frame → Annex-B NAL |
| `openMp4(fs\|sd, path, pcm?, rate?)` | Preferred: write MP4 on close |
| `openFile(fs\|sd, path)` / `encodeToFile` / `closeFile` | File encode path |
| `fileOpen` / `fileBytes` / `filePath` / `recordElapsedMs` | File status |

**Remux helpers (`ESP32P4_H264Mp4.h`):**

| API | Meaning |
| --- | --- |
| `esp32p4_h264_annexb_to_mp4(fs, annexb, mp4, w, h, duration_ms)` | Annex-B → MP4 |
| `…(+ pcm_path, rate, channels)` | Fuse PCM audio track |

---

### Mic — `ESP32P4_Mic`

| API | Meaning |
| --- | --- |
| `begin(sample_rate)` / `end` / `ready` / `sampleRate` | Init ES8311 |
| `poll()` | Drain I2S, update levels, append PCM |
| `startPcmFile(fs\|sd, path)` / `stopPcmFile` / … | Raw PCM to storage |
| `setGain` / `gain` | Capture gain 0–100 |
| `rms` / `peak` / `copyWave` | Levels / waveform bins |

---

### CV imgproc — `ESP32P4_Cv` (static)

| API | Meaning |
| --- | --- |
| `toGray` / `toGrayScale` / `downsample2x` | RGB565 → gray |
| `blur3x3` | Box blur |
| `threshold` / `otsu` / `adaptiveThreshold` | Binarize |
| `erode` / `dilate` / `morphologyOpen` / `morphologyClose` | Morphology |
| `rgb565ToHsv` / `inRangeHsv` | HSV pixel / mask |
| `edges` | Sobel + dual threshold |
| `findBlobs` | Connected components → `esp32p4_blob_t` |
| `line` / `circle` / `putText` / `drawBlob` | Draw on RGB565 |

**Types:** `esp32p4_hsv_t`, `esp32p4_blob_t`, `esp32p4_thresh_t`

---

### CV dashboard — `ESP32P4_CvDash`

| API | Meaning |
| --- | --- |
| `applyPreset(cfg, preset)` | HSV/mode presets (red, green, coins, …) |
| `process(rgb, w, h, cfg)` | Run mode on stream frame (in place) |
| `release()` | Free PSRAM scratch |

**Modes:** `OFF`, `BLOBS`, `MASK`, `EDGES`, `THRESH`, `GRAY`, `BLUR`, `EDGE_TRACK`  
**Config:** `esp32p4_cv_dash_cfg_t` (HSV ranges, morph, areas, `blobs`/`tracks`/`proc_ms`)

---

### Tracker — `ESP32P4_Tracker`

| API | Meaning |
| --- | --- |
| `reset()` | Clear tracks |
| `update(blobs, n, max_dist, max_lost)` | Associate IDs across frames |
| `count` / `track(i)` | Read tracks |
| `draw(img, …)` | Draw tracked boxes |

**Type:** `esp32p4_track_t` `{ id, cx, cy, w, h, area, age, lost, active }`

---

### OpenCV core — `esp_cv` / `cv`

```cpp
esp_cv::Mat m(240, 320, esp_cv::CV_8UC1);           // PSRAM-owned
esp_cv::Mat view = m(esp_cv::Rect(10, 10, 64, 64)); // ROI view
esp_cv::Mat wrap = esp_cv::wrapRgb565(buf, w, h);   // external FB
// Optional: cv::Mat, cv::Point, …  (unless ESP_CV_NO_CV_ALIAS)
```

| API | Meaning |
| --- | --- |
| `Point` `Point2f` `Size` `Rect` `Scalar` `Range` | Geometry |
| `CV_8UC1` / `CV_8UC2` / `CV_8UC3` / … | Dtypes (gray / RGB565 / RGB888) |
| `Mat::create` / `release` / `zeros` | Alloc/free in PSRAM |
| `Mat(…, data, step)` | Wrap external buffer (zero-copy) |
| `operator()(Rect)` / `rowRange` / `colRange` | ROI / submatrix |
| `clone` / `copyTo` / `setTo` | Deep copy / fill |
| `ptr` / `empty` / `isContinuous` / `step` | Access |
| `wrapRgb565` / `wrapGray` / `wrapRgb888` | Convenience wraps |
| `validImageSize` / `validType` | Validation |

Imgproc still via `ESP32P4_Cv` + pointers; `Mat` is the buffer/ROI layer.

---

### Face detect — `ESP32P4_FaceDetect`

| API | Meaning |
| --- | --- |
| `begin(model)` / `end` / `ready` | Load detector models from VFS mount |
| `detect(fb\|rgb565, out, max)` | Run detect → `esp32p4_face_t[]` |
| `draw(...)` | Boxes on RGB565 |

**Models:** `MSRMNP_S8_V1`, `ESPDET_PICO_224`, `ESPDET_PICO_416`  
**`esp32p4_face_t`:** score, box, landmarks[5]

---

### Face AI — `ESP32P4_FaceAi`

| API | Meaning |
| --- | --- |
| `begin(model, db_path, names_path)` | Detect + optional recognize DB |
| `end` / `ready` / `recognitionReady` | Status |
| `setDetModel` | Switch detector |
| `run(rgb\|fb, out, max, recognize)` | Detect (+ match DB) → `esp32p4_face_id_t[]` |
| `requestEnroll(name)` / `cancelEnroll` / `enrollPending` | Multi-sample enroll |
| `enrollSamplesDone` / `enrollSamplesNeed` / `enroll(...)` | Enroll progress |
| `clearDb` / `deleteId` / `deleteName` | Manage features |
| `setName` / `nameOf` / `rosterText` | Names / UI roster |
| `setThresh` / `thresh` | Similarity threshold |
| `draw(...)` | Boxes + labels |
| `featCount` / `lastMs` / `lastCount` / `lastEnrollId` / `lastEnrollStatus` | Status |
| `detModel` / `dbPath` | Config |

DB paths: VFS `/sdcard/face/...` · card-relative `/face/...`

---

### Vision AI helpers — `ESP32P4_VisionAi` (static)

| API | Meaning |
| --- | --- |
| `letterboxRgb565(...)` | RGB565 → model RGB888 + meta |
| `mapBoxToSrc` / `mapPointToSrc` | Model coords → source pixels |
| `nms` / `iou` | Non-max suppression |
| `softmax` | Softmax logits |
| `drawDets` | Draw detections |

**Types:** `esp32p4_det_t`, `esp32p4_keypoint_t`, `esp32p4_pose_t`, `esp32p4_letterbox_t`

---

### Motion DSP — `ESP32P4_Dsp`

| API | Meaning |
| --- | --- |
| `begin(w, h, threshold)` / `end` | Init frame-diff |
| `detect(fb, &out)` | Motion flag + ROI |

**Type:** `esp32p4_motion_t` `{ moving, changed, total, roi }`

---

### WHO pipeline — `ESP32P4_WhoPipeline`

| API | Meaning |
| --- | --- |
| `begin(cam, queue_len)` / `end` | Async capture pipeline |
| `onFrame(cb, ctx)` | Callback per frame |
| `waitFrame(&out, timeout)` | Blocking dequeue |
| `running()` | Task alive |

---

### Image utils — `ESP32P4_Img` (static)

| API | Meaning |
| --- | --- |
| `rgb565ToRgb888` / `rgb888ToRgb565` | Format convert |
| `luma565` / `histogram565` | Luma / 16-bin hist |
| `crop565` / `downsample2x565` | Crop / 2× down |
| `fillRect565` | SW rect / outline |
| `blit565` | Blit FB into buffer |

**Type:** `esp32p4_rect_t { x, y, w, h }`

---

### WebFileManager

| API | Meaning |
| --- | --- |
| `WebFileManager(primaryStorage)` | Construct |
| `addVolume(name, storage)` | Extra volume (multi-FS) |
| `setPorts(ui, file)` / `setName` / `setHomePort` / `setAuth` | Config (chainable) |
| `begin` / `loop` | Start servers / pump |
| `startFileTask(...)` | Background file HTTP task |
| `started` / `authEnabled` / `uiPort` / `filePort` / `homePort` | Status |
| `volumeCount` / `volume(i)` | Volumes |
| `refreshUsageAsync` | Rescan used/free |

**Storage backends:** `WfmStorageFS`, `WfmStorageSD`, `WfmStorageSD_MMC`, `WfmStorageFFat`, `WfmStorageLittleFS`, `WfmStorageSPIFFS`  
**Network helper:** `WfmNetwork::beginSTA` / `beginAP` / `beginEthernet` / `guitionM3Eth` / `ready` / `localIP`  
Typical ports: camera `:80`, WFM UI `:82`, transfers `:83`

---

### PSRAM — free helpers

| API | Meaning |
| --- | --- |
| `esp32p4_psram_alloc(bytes, align?)` | Aligned alloc (PSRAM preferred) |
| `esp32p4_psram_free(ptr)` | Free |
| `esp32p4_psram_msync(ptr, bytes)` | Cache sync |
| `esp32p4_psram_available` / `esp32p4_psram_free_size` | Health |

---

## Examples

| Sketch | What it shows |
| --- | --- |
| `01_CamTest` | Capture loop |
| `02_JpegSnapshot` | HW JPEG |
| `04_WiFiMjpeg` | Wi‑Fi MJPEG UI |
| `09_SdCard` | SD mount |
| `10_WiFiMjpegSdCapture` | Capture → `/IMG` (`APP_STORAGE`) |
| `11`–`12` H264 | Record MP4 (`APP_STORAGE`) |
| `14_EthSdBrowser` | WebFileManager multi-volume (SD + FFat + LittleFS) |
| `15` / `16` mic | WAV to storage + optional FFat volume |
| `17` / `18_EthH264Record*` | Eth + record + mic (+ files, `APP_STORAGE`) |
| `19_CvColorBlobs` / `20_EthCvPreview` | HSV blobs / live CV |
| `21_EthFaceWeb` | Eth + face + settings + capture/record + WFM (`APP_STORAGE`) |

Arduino IDE: **File → Examples → ESP32CSI_Vision → …**

---

## Layout

```text
src/
  ESP32CSI_Vision.h     ← umbrella include
  cam/ jpeg/ ppa/ dsp/  ← capture / encode / scale / motion
  stream/               ← MJPEG + UI
  cv/ opencv/           ← imgproc + Mat core
  face/ espdl/          ← Face AI + vendored ESP-DL
  h264/ audio/ sd/      ← record / mic / SD
  storage/              ← StoragePref + model mount
  wfm/                  ← WebFileManager
examples/               ← Arduino sketches
```

---

## Notes

- Always `cam.release(fb)` after `capture()`.
- Keep MJPEG on `:81` so `/control` on `:80` stays responsive.
- Avoid heavy browse/upload while recording on the same volume.
- Face models must be on the preferred volume before `FaceAi.begin()`; WFM can upload them.
- For FFat: select a partition scheme with a FAT data partition (e.g. `app3M_fat9M_16MB`).
- Class prefix stays `ESP32P4_*` for API stability; library name is CSI-oriented.

---

## License

MIT · [Rakib Hasan](https://github.com/thezerohz) ([@thezerohz](https://github.com/thezerohz)) · [repo](https://github.com/thezerohz/ESP32CSI_Vision)
