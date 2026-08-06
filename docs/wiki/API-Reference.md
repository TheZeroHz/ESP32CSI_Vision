# API Reference

Complete public API for **ESP32CSI_Vision** v3.1.0.  
Umbrella header: `#include <ESP32CSI_Vision.h>`

Jump: [Camera](#esp32p4_camera) · [Config](#camera-config--helpers) · [JPEG](#esp32p4_jpeg) · [PPA](#esp32p4_ppa) · [Img](#esp32p4_img) · [DSP](#esp32p4_dsp) · [MJPEG](#esp32p4_mjpegserver) · [WHO](#esp32p4_whopipeline) · [PSRAM](#psram-helpers) · [FaceDetect](#esp32p4_facedetect-esp-idf-only)

---

## ESP32P4_Camera

MIPI CSI + ISP capture into PSRAM framebuffers (RGB565).

**Header:** `cam/ESP32P4_Camera.h`  
**Alias:** `ESP32P4_CSI_Camera`

### Lifecycle

#### `bool begin(esp32p4_board_t board = ESP32P4_BOARD_GUITION_M3)`

| Param | Type | Default | Description |
| --- | --- | --- | --- |
| `board` | `esp32p4_board_t` | `ESP32P4_BOARD_GUITION_M3` | Pin/LDO/sensor preset |

**Returns:** `true` if sensor probed, FBs allocated, CSI streaming.

```cpp
ESP32P4_Camera cam;
if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
  Serial.println("begin failed");
}
```

#### `bool begin(const esp32p4_cam_config_t &cfg)`

| Param | Type | Description |
| --- | --- | --- |
| `cfg` | `const esp32p4_cam_config_t &` | Full custom config (see [Config](#camera-config--helpers)) |

```cpp
esp32p4_cam_config_t cfg = esp32p4_cam_config_default();
cfg.fb_count = 3;
cfg.sensor = ESP32P4_SENSOR_OV5647;
cfg.test_pattern = false;
if (!cam.begin(cfg)) { /* fail */ }
```

#### `void end()`

Stops CSI/ISP, frees LDO/FB resources. Safe to call if not started.

---

### Capture

#### `camera_fb_t *capture(uint32_t timeout_ms = 2000)`

| Param | Type | Default | Description |
| --- | --- | --- | --- |
| `timeout_ms` | `uint32_t` | `2000` | Max wait for a new frame |

**Returns:** Pointer to a filled `camera_fb_t`, or `nullptr` on timeout.  
**Must call** `release(fb)` when done.

```cpp
camera_fb_t *fb = cam.capture(500);  // short timeout for streaming loops
if (!fb) {
  Serial.println("timeout");
  return;
}
// use fb->buf, fb->width, fb->height, fb->len
cam.release(fb);
```

#### `void release(camera_fb_t *fb)`

| Param | Type | Description |
| --- | --- | --- |
| `fb` | `camera_fb_t *` | Frame from `capture()` (nullptr-safe in practice — always pass what you got) |

Returns the framebuffer to the pool so CSI can refill it.

---

### Sensor preferences (set)

All return `bool` (`true` = applied). OV5647 path is the primary implementation.

#### `bool setTestPattern(bool enable)`

| Param | Description |
| --- | --- |
| `enable` | `true` = sensor test pattern / colorbar |

```cpp
cam.setTestPattern(true);   // debug cable / ISP
```

#### `bool setHMirror(bool enable)` / `bool setVFlip(bool enable)`

| Param | Description |
| --- | --- |
| `enable` | Horizontal mirror / vertical flip |

```cpp
cam.setHMirror(true);
cam.setVFlip(false);
```

#### `bool setAEC(bool enable)` / `bool setAGC(bool enable)`

| Param | Description |
| --- | --- |
| `enable` | Auto exposure / auto gain |

```cpp
cam.setAEC(false);
cam.setAGC(false);
cam.setExposure(800);
cam.setGain(16);
```

#### `bool setExposure(uint16_t lines)`

| Param | Type | Description |
| --- | --- | --- |
| `lines` | `uint16_t` | Manual exposure (sensor line units; meaningful when AEC off) |

#### `bool setGain(uint16_t gain)`

| Param | Type | Description |
| --- | --- | --- |
| `gain` | `uint16_t` | Manual gain (when AGC off) |

#### `bool setGainCeiling(uint16_t ceiling)`

| Param | Type | Description |
| --- | --- | --- |
| `ceiling` | `uint16_t` | Max gain the AGC path may use |

---

### Sensor preferences (get)

| Method | Param | Returns |
| --- | --- | --- |
| `bool getHMirror(bool *out) const` | out pointer | writes current mirror |
| `bool getVFlip(bool *out) const` | out pointer | writes current flip |
| `bool getAEC(bool *out) const` | out pointer | AEC on/off |
| `bool getAGC(bool *out) const` | out pointer | AGC on/off |
| `bool getExposure(uint16_t *lines) const` | out pointer | exposure lines |
| `bool getGain(uint16_t *gain) const` | out pointer | gain |
| `bool getGainCeiling(uint16_t *ceiling) const` | out pointer | ceiling |

```cpp
bool aec = true;
uint16_t exp = 0;
if (cam.getAEC(&aec) && cam.getExposure(&exp)) {
  Serial.printf("aec=%d exp=%u\n", aec, exp);
}
```

#### `bool testPattern() const`

Cached flag whether test pattern was requested via config/`setTestPattern`.

---

### Status getters

| Method | Returns | Description |
| --- | --- | --- |
| `uint16_t width() const` | pixels | Active frame width |
| `uint16_t height() const` | pixels | Active frame height |
| `int sensorAddress() const` | I²C addr | `0` if not detected |
| `bool detected() const` | bool | `sensorAddress() > 0` |
| `const char *sensorName() const` | C string | e.g. `"OV5647 (OV CSI)"` |
| `uint32_t newTransCount() const` | count | CSI new-transaction counter |
| `uint32_t doneCount() const` | count | CSI done counter |
| `uint8_t fbCount() const` | count | Allocated framebuffers |
| `bool psramOk() const` | bool | PSRAM usable for FBs |

```cpp
Serial.printf("%s @0x%02X  %ux%u  fb=%u  psram=%s\n",
              cam.sensorName(), cam.sensorAddress(),
              cam.width(), cam.height(), cam.fbCount(),
              cam.psramOk() ? "ok" : "NO");
```

---

## Camera config & helpers

### `esp32p4_cam_config_t`

| Field | Type | Typical | Description |
| --- | --- | --- | --- |
| `sda` | `int` | `7` | I²C SDA GPIO |
| `scl` | `int` | `8` | I²C SCL GPIO |
| `xclk` | `int` | `-1` | External XCLK GPIO (`-1` = unused / internal) |
| `pwdn` | `int` | `-1` | Power-down GPIO |
| `reset` | `int` | `-1` | Reset GPIO |
| `xclk_hz` | `uint32_t` | `24000000` | Sensor clock Hz |
| `i2c_addr` | `int` | `0` | Force addr; `0` = probe |
| `ldo_chan` | `int` | `3` | MIPI PHY LDO channel |
| `ldo_mv` | `int` | `2500` | LDO millivolts |
| `frame_size` | `esp32p4_cam_framesize_t` | `800×640` | Native CSI size |
| `pixel_format` | `esp32p4_cam_pixformat_t` | `RGB565` | Output format |
| `lane_bit_rate_mbps` | `int` | `200` | CSI lane bitrate |
| `sensor` | `esp32p4_cam_sensor_t` | `OV5647` | Sensor selection |
| `test_pattern` | `bool` | `false` | Start with pattern |
| `fb_count` | `uint8_t` | `2` | PSRAM FBs (max `ESP32P4_CAM_FB_MAX`, default 3) |

### Helpers

#### `esp32p4_cam_config_t esp32p4_cam_config_default()`

Same as Guition M3 board defaults.

#### `esp32p4_cam_config_t esp32p4_cam_config_board(esp32p4_board_t board)`

| Param | Description |
| --- | --- |
| `board` | Preset id (pins currently share Guition-style defaults in v3.1; use custom `cfg` for other wiring) |

```cpp
esp32p4_cam_config_t cfg = esp32p4_cam_config_board(ESP32P4_BOARD_GUITION_M3);
cfg.fb_count = 3;
cfg.lane_bit_rate_mbps = 200;
cam.begin(cfg);
```

### `camera_fb_t`

| Field | Type | Description |
| --- | --- | --- |
| `buf` | `uint8_t *` | Pixel data (RGB565: 2 bytes/pixel) |
| `len` | `size_t` | Byte length (`width * height * 2` for RGB565) |
| `width` | `uint16_t` | Width |
| `height` | `uint16_t` | Height |
| `format` | `esp32p4_cam_pixformat_t` | Pixel format |
| `timestamp_us` | `uint32_t` | Capture timestamp (µs) |

```cpp
camera_fb_t *fb = cam.capture();
if (fb && fb->format == ESP32P4_PIXFORMAT_RGB565) {
  const uint16_t *px = (const uint16_t *)fb->buf;
  uint16_t center = px[(fb->height / 2) * fb->width + (fb->width / 2)];
  Serial.printf("center=0x%04X\n", center);
}
cam.release(fb);
```

---

## ESP32P4_Jpeg

Hardware JPEG encode / decode.

**Header:** `jpeg/ESP32P4_Jpeg.h`

### `bool begin(uint16_t max_w = 800, uint16_t max_h = 640, uint8_t quality = 45)`

| Param | Type | Default | Description |
| --- | --- | --- | --- |
| `max_w` | `uint16_t` | `800` | Max encode width to size internal buffers |
| `max_h` | `uint16_t` | `640` | Max encode height |
| `quality` | `uint8_t` | `45` | Initial quality **1–100** (higher = larger/sharper) |

**Returns:** `true` if HW JPEG engine ready.

```cpp
ESP32P4_Jpeg jpeg;
jpeg.begin(cam.width(), cam.height(), 45);
```

### `void end()`

Release encoder/decoder resources.

### `size_t encode(const camera_fb_t *fb, uint8_t *out, size_t out_cap)`

| Param | Type | Description |
| --- | --- | --- |
| `fb` | `const camera_fb_t *` | RGB565 frame |
| `out` | `uint8_t *` | Destination JPEG buffer (prefer PSRAM) |
| `out_cap` | `size_t` | Capacity of `out` |

**Returns:** JPEG byte count, or `0` on failure.

```cpp
uint8_t *jpg = (uint8_t *)esp32p4_psram_alloc(200 * 1024);
camera_fb_t *fb = cam.capture();
size_t n = jpeg.encode(fb, jpg, 200 * 1024);
cam.release(fb);
Serial.printf("jpeg %u bytes\n", (unsigned)n);
```

### `size_t encode(const uint8_t *rgb565, uint16_t w, uint16_t h, uint8_t *out, size_t out_cap)`

| Param | Description |
| --- | --- |
| `rgb565` | Raw RGB565 buffer |
| `w`, `h` | Dimensions |
| `out`, `out_cap` | JPEG destination |

Same return as above — use after PPA scale when you no longer have a `camera_fb_t`.

### `void setQuality(uint8_t q)`

| Param | Range | Description |
| --- | --- | --- |
| `q` | 1–100 | Clamped; applies to next `encode` |

```cpp
jpeg.setQuality(60);
```

### `bool decodeInfo(const uint8_t *jpg, size_t jpg_len, uint32_t *w, uint32_t *h)`

| Param | Description |
| --- | --- |
| `jpg`, `jpg_len` | JPEG blob |
| `w`, `h` | Out: decoded dimensions |

**Returns:** `true` if SOF parsed.

### `size_t decode(const uint8_t *jpg, size_t jpg_len, uint8_t *rgb_out, size_t out_cap, uint32_t *w, uint32_t *h)`

| Param | Description |
| --- | --- |
| `jpg`, `jpg_len` | Input JPEG |
| `rgb_out`, `out_cap` | RGB output buffer + capacity |
| `w`, `h` | Out dimensions |

**Returns:** Bytes written (0 on fail). See example `03_JpegDecode`.

---

## ESP32P4_Ppa

Hardware Pixel Processing Accelerator — scale / rotate / mirror RGB565.

**Header:** `ppa/ESP32P4_Ppa.h`

### `bool begin()` / `void end()`

Init / tear down PPA client.

### `bool scale(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, uint16_t dst_w, uint16_t dst_h)`

| Param | Type | Description |
| --- | --- | --- |
| `src` | `const camera_fb_t *` | Source frame |
| `dst` | `uint8_t *` | Destination RGB565 buffer |
| `dst_cap` | `size_t` | Must be ≥ `dst_w * dst_h * 2` |
| `dst_w` | `uint16_t` | Output width |
| `dst_h` | `uint16_t` | Output height |

**Returns:** `true` on success.

```cpp
ESP32P4_Ppa ppa;
ppa.begin();
uint8_t *scaled = (uint8_t *)esp32p4_psram_alloc(400 * 320 * 2);
camera_fb_t *fb = cam.capture();
if (ppa.scale(fb, scaled, 400 * 320 * 2, 400, 320)) {
  Serial.println("scaled ok");
}
cam.release(fb);
```

### `bool rotate90(const camera_fb_t *src, uint8_t *dst, size_t dst_cap)`

90° clockwise. Destination size is swapped (`h×w`). `dst_cap` must fit `src->height * src->width * 2`.

### `bool mirror(const camera_fb_t *src, uint8_t *dst, size_t dst_cap, bool mx, bool my)`

| Param | Description |
| --- | --- |
| `mx` | Mirror X (horizontal) |
| `my` | Mirror Y (vertical) |

Same dimensions as source.

```cpp
ppa.mirror(fb, dst, dst_cap, true, false);  // h-mirror only
```

---

## ESP32P4_Img

CPU pixel utilities (no HW). All **static** methods.

**Header:** `img/ESP32P4_Img.h`

### `esp32p4_rect_t`

| Field | Type | Description |
| --- | --- | --- |
| `x`, `y` | `int` | Top-left |
| `w`, `h` | `int` | Size |

### Methods

| Method | Params | Description |
| --- | --- | --- |
| `rgb565ToRgb888` | `src`, `dst`, `pixels` | Expand to 3 bytes/pix |
| `rgb888ToRgb565` | `src`, `dst`, `pixels` | Pack to 2 bytes/pix |
| `luma565` | `px` | Approximate luma of one RGB565 pixel |
| `histogram565` | `src`, `pixels`, `bins[16]` | 16-bin luma histogram |
| `crop565` | `src`, `sw`, `sh`, `r`, `dst` | Crop rectangle to `dst` |
| `downsample2x565` | `src`, `sw`, `sh`, `dst` | 2× box downsample (CPU fallback) |
| `fillRect565` | `img`, `w`, `h`, `r`, `color`, `thickness=2` | Draw rect outline (`thickness` default 2) |
| `blit565` | `fb`, `dst`, `dw`, `dh` | Blit frame into destination canvas |

```cpp
esp32p4_rect_t box{100, 80, 120, 120};
ESP32P4_Img::fillRect565((uint16_t *)fb->buf, fb->width, fb->height, box, 0xF800, 3); // red

uint32_t bins[16] = {};
ESP32P4_Img::histogram565((const uint16_t *)fb->buf, fb->width * fb->height, bins);
```

---

## ESP32P4_Dsp

Frame-difference motion detection on RGB565.

**Header:** `dsp/ESP32P4_Dsp.h`

### `esp32p4_motion_t`

| Field | Type | Description |
| --- | --- | --- |
| `moving` | `bool` | Motion above threshold |
| `changed` | `uint32_t` | Changed sample count |
| `total` | `uint32_t` | Total samples compared |
| `roi` | `esp32p4_rect_t` | Bounding box of change |

### `bool begin(uint16_t w, uint16_t h, uint8_t threshold = 25)`

| Param | Type | Default | Description |
| --- | --- | --- | --- |
| `w`, `h` | `uint16_t` | — | Frame size (match camera) |
| `threshold` | `uint8_t` | `25` | Per-pixel luma delta threshold (lower = more sensitive) |

### `bool detect(const camera_fb_t *fb, esp32p4_motion_t *out)`

| Param | Description |
| --- | --- |
| `fb` | Current frame |
| `out` | Filled motion result |

**Returns:** `true` if comparison ran (first frame may seed background only).

```cpp
ESP32P4_Dsp dsp;
dsp.begin(cam.width(), cam.height(), 22);

camera_fb_t *fb = cam.capture();
esp32p4_motion_t m{};
if (dsp.detect(fb, &m) && m.moving) {
  Serial.printf("motion roi=%d,%d %dx%d  %u/%u\n",
                m.roi.x, m.roi.y, m.roi.w, m.roi.h, m.changed, m.total);
}
cam.release(fb);
```

### `void end()`

Frees previous-frame buffer.

---

## ESP32P4_MjpegServer

Dual-port Wi‑Fi webcam: **control UI on `port`**, **MJPEG on `port+1`**.

**Header:** `stream/ESP32P4_Mjpeg.h`  
See also [HTTP & Preferences](HTTP-and-Preferences.md).

### `bool begin(ESP32P4_Camera *cam, uint16_t port = 80, uint8_t quality = 35)`

| Param | Type | Default | Description |
| --- | --- | --- | --- |
| `cam` | `ESP32P4_Camera *` | — | Already-started camera |
| `port` | `uint16_t` | `80` | Control / UI / `/jpg` port |
| `quality` | `uint8_t` | `35` | Initial JPEG quality (lower = faster) |

**Side effect:** stream port = `port + 1` (e.g. **81**).

```cpp
ESP32P4_MjpegServer stream;
stream.begin(&cam, 80, 35);
Serial.printf("UI http://%s/\n", WiFi.localIP().toString().c_str());
Serial.printf("stream http://%s:%u/stream\n",
              WiFi.localIP().toString().c_str(), stream.streamPort());
```

### `void loop()`

Call from Arduino `loop()`. HTTP runs in FreeRTOS tasks; this yields (`delay(1)`).

### `void end()`

Stops workers, HTTP servers, frees JPEG/PPA buffers.

### Live preferences

| Method | Param | Description |
| --- | --- | --- |
| `void setQuality(uint8_t q)` | 1–100 | JPEG quality |
| `void setFrameSkip(uint8_t skip)` | 0+ | Skip N frames between encodes |
| `bool setFramesize(uint8_t fs)` | `0`–`4` | PPA output size enum |

```cpp
stream.setQuality(50);
stream.setFrameSkip(1);
stream.setFramesize(ESP32P4_STREAM_QVGA);  // 320x240
```

### Getters

| Method | Returns |
| --- | --- |
| `uint32_t sent() const` | Frames sent on stream |
| `uint32_t lastJpegBytes() const` | Last JPEG size |
| `uint8_t quality() const` | Current quality |
| `uint8_t frameSkip() const` | Current skip |
| `uint8_t framesize() const` | Current framesize enum |
| `uint16_t outWidth() / outHeight()` | Encoded output size |
| `uint16_t controlPort() const` | UI port |
| `uint16_t streamPort() const` | MJPEG port |

---

## ESP32P4_WhoPipeline

ESP-WHO–style async capture queue (Arduino-safe; **no** ESP-DL).

**Header:** `who/ESP32P4_Who.h`

### Types

```cpp
struct esp32p4_who_fb_t {
  uint8_t *buf;
  size_t len;
  uint16_t width;
  uint16_t height;
  uint32_t timestamp_us;
  void *user;
};

typedef void (*esp32p4_who_cb_t)(const esp32p4_who_fb_t *fb, void *ctx);
```

### `void onFrame(esp32p4_who_cb_t cb, void *ctx = nullptr)`

| Param | Description |
| --- | --- |
| `cb` | Callback invoked when a frame is queued |
| `ctx` | User pointer passed to callback |

Call **before** `begin()`.

### `bool begin(ESP32P4_Camera *cam, uint8_t queue_len = 2)`

| Param | Default | Description |
| --- | --- | --- |
| `cam` | — | Started camera |
| `queue_len` | `2` | FreeRTOS queue depth |

Starts a background task that captures and posts frames.

### `bool waitFrame(esp32p4_who_fb_t *out, uint32_t timeout_ms = 1000)`

| Param | Default | Description |
| --- | --- | --- |
| `out` | — | Filled with frame metadata |
| `timeout_ms` | `1000` | Wait for queue item |

**Returns:** `true` if a frame was received.

### `bool running() const` / `void end()`

Pipeline status / stop task + queue.

```cpp
ESP32P4_WhoPipeline who;

static void on_frame(const esp32p4_who_fb_t *fb, void *) {
  Serial.printf("cb %ux%u\n", fb->width, fb->height);
}

void setup() {
  cam.begin(ESP32P4_BOARD_GUITION_M3);
  who.onFrame(on_frame);
  who.begin(&cam, 2);
}

void loop() {
  esp32p4_who_fb_t fb{};
  if (who.waitFrame(&fb, 2000)) {
    Serial.printf("wait %ux%u\n", fb.width, fb.height);
  }
}
```

---

## PSRAM helpers

**Header:** `mem/ESP32P4_Psram.h`

| Function | Params | Returns / notes |
| --- | --- | --- |
| `void *esp32p4_psram_alloc(size_t bytes, size_t align = ESP32P4_CACHE_ALIGN)` | bytes, align (default **128**) | PSRAM pointer or `nullptr` |
| `void esp32p4_psram_free(void *ptr)` | pointer | Free |
| `void esp32p4_psram_msync(void *ptr, size_t bytes)` | region | Cache sync before DMA/HW use |
| `bool esp32p4_psram_available()` | — | PSRAM present |
| `size_t esp32p4_psram_free_size()` | — | Free PSRAM bytes |

```cpp
if (!esp32p4_psram_available()) {
  Serial.println("Enable PSRAM in board menu!");
}
void *p = esp32p4_psram_alloc(100 * 1024);
esp32p4_psram_msync(p, 100 * 1024);
esp32p4_psram_free(p);
```

Macro: `ESP32P4_CACHE_ALIGN` = `128` (override before include if needed).

---

## ESP32P4_FaceDetect (ESP-IDF only)

**Not available in Arduino IDE** (no esp-dl in prebuilt SDK).  
**Header:** `idf_src/ESP32P4_FaceDetect.h`  
**Example:** `idf_examples/08_FaceDetect`

### `esp32p4_face_t`

| Field | Type | Description |
| --- | --- | --- |
| `score` | `float` | Confidence |
| `x`, `y`, `w`, `h` | `int` | Bounding box |
| `landmarks[5][2]` | `int` | eyeL, mouthL, nose, eyeR, mouthR |
| `has_landmarks` | `bool` | Landmark valid |

### Models (`ESP32P4_FaceDetect::Model`)

| Enum | Value | Notes |
| --- | :---: | --- |
| `MSRMNP_S8_V1` | 0 | Default two-stage, good P4 speed |
| `ESPDET_PICO_224` | 1 | Pico 224 |
| `ESPDET_PICO_416` | 2 | Pico 416 |

### `bool begin(Model model = MSRMNP_S8_V1)`

Loads HumanFaceDetect weights (flash rodata by default).

### `int detect(const camera_fb_t *fb, esp32p4_face_t *out, int max_out)`

| Param | Description |
| --- | --- |
| `fb` | RGB565 frame |
| `out` | Array of faces |
| `max_out` | Cap (array length) |

**Returns:** Number of faces written (`0`…`max_out`).

### `bool ready() const` / `void end()`

```cpp
#include "ESP32CSI_Vision.h"
#include "ESP32P4_FaceDetect.h"

ESP32P4_FaceDetect face;
esp32p4_face_t faces[8];

face.begin(ESP32P4_FaceDetect::MSRMNP_S8_V1);
camera_fb_t *fb = cam.capture();
int n = face.detect(fb, faces, 8);
for (int i = 0; i < n; i++) {
  Serial.printf("[%d] %.2f @%d,%d %dx%d\n",
                i, faces[i].score, faces[i].x, faces[i].y, faces[i].w, faces[i].h);
}
cam.release(fb);
```

Build:

```bat
cd idf_examples/08_FaceDetect
idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
idf.py -p COMx build flash monitor
```

---

## Compile-time preferences

| Macro | Default | Effect |
| --- | --- | --- |
| `ESP32P4_CAM_FB_MAX` | `3` | Max FB slots in `ESP32P4_Camera` |
| `ESP32P4_CACHE_ALIGN` | `128` | Default PSRAM alignment |
| `CONFIG_IDF_TARGET_ESP32P4` | from board | Required; else `#error` |

```cpp
#define ESP32P4_CAM_FB_MAX 3
#include <ESP32CSI_Vision.h>
```

---

## End-to-end recipe (capture → JPEG → optional stream)

```cpp
#include <WiFi.h>
#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;
ESP32P4_Jpeg jpeg;
ESP32P4_MjpegServer stream;

void setup() {
  Serial.begin(115200);
  cam.begin(ESP32P4_BOARD_GUITION_M3);
  jpeg.begin(cam.width(), cam.height(), 40);

  // … WiFi.begin … then either manual JPEG:
  //   encode + HTTP yourself
  // or:
  stream.begin(&cam, 80, 35);
}

void loop() {
  stream.loop();
}
```

Next: [HTTP & Preferences](HTTP-and-Preferences.md) · [Enums & Types](Enums-and-Types.md)
