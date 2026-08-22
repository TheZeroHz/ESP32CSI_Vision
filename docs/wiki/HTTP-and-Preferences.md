# HTTP API & Preferences

Live controls for **`ESP32P4_MjpegServer`** (examples `04_WiFiMjpeg`, `30_EthLiveAvFiles`, detect web sketches `32`–`41`).

SSID / C6 pins / Ethernet PHY live in the sketch’s `board_config.h` (`esp32csi_wifi_begin()` / `esp32csi_eth_begin()`), not in this page.

Sketch-owned preview (`ESP32P4_WebPreview`, example `43`) is a smaller HTTP surface — see [WebPreview](#webpreview-example-43) at the bottom.

Class methods: [API Reference — MJPEG](API-Reference.md#esp32p4_mjpegserver).

## Ports (important)

| Port | Role | Blocks? |
| :---: | --- | --- |
| **80** (default `port`) | UI `/`, `/control`, `/status`, `/jpg`, `/capture` | No — stays responsive |
| **81** (`port + 1`) | `/stream` multipart MJPEG | Yes for that connection only |

```text
http://<ip>/              → settings UI
http://<ip>:81/stream     → live video
```

Change base port:

```cpp
stream.begin(&cam, 8080, 35);  // UI :8080, stream :8081
```

---

## Endpoints

### `GET /` — Settings UI

HTML page with preview (loads stream from `:81`), sliders, toggles.

### `GET /stream` — MJPEG (port 81)

`multipart/x-mixed-replace` JPEG stream.

```bash
# curl one part is awkward; use browser or:
python examples/04_WiFiMjpeg/cam_wifi_viewer.py 192.168.0.3 81
```

### `GET /jpg` and `GET /capture` — Single snapshot

Returns one JPEG (`image/jpeg`) from the latest encoded frame.

```bash
curl -o shot.jpg "http://192.168.0.3/jpg"
curl -o shot.jpg "http://192.168.0.3/capture"
```

### `GET /status` — JSON preferences snapshot

```bash
curl "http://192.168.0.3/status"
```

Example payload:

```json
{
  "sensor": "OV5647 (OV CSI)",
  "framesize": 3,
  "out_w": 800,
  "out_h": 640,
  "w": 800,
  "h": 640,
  "native_w": 800,
  "native_h": 640,
  "quality": 35,
  "frameskip": 0,
  "jpeg": 42100,
  "encode_ms": 12,
  "sent": 1042,
  "dropped": 3,
  "psram": 28000000,
  "control_port": 80,
  "stream_port": 81,
  "hmirror": 0,
  "vflip": 0,
  "aec": 1,
  "agc": 1,
  "aec_value": 800,
  "agc_gain": 16,
  "gainceiling": 16,
  "colorbar": 0
}
```

| Field | Meaning |
| --- | --- |
| `framesize` | Stream enum 0–9 (PPA output) |
| `out_w` / `out_h` | Encoded stream size |
| `native_w` / `native_h` | CSI capture size |
| `quality` | JPEG quality |
| `frameskip` | Frames skipped between encodes |
| `jpeg` | Last JPEG byte size |
| `encode_ms` | Last encode time |
| `sent` / `dropped` | Stream counters |
| `psram` | Free PSRAM bytes |
| `hmirror`…`colorbar` | Live sensor prefs |

---

### `GET /control` — Set a preference

**Query:**

| Param | Required | Description |
| --- | :---: | --- |
| `var` | yes | Preference name (string) |
| `val` | yes | Integer value |

```text
http://<ip>/control?var=<name>&val=<int>
```

**Response:** `200` on success, `400` if unsupported/failed.

```bash
curl "http://192.168.0.3/control?var=quality&val=50"
curl "http://192.168.0.3/control?var=framesize&val=7"   # QVGA
curl "http://192.168.0.3/control?var=hmirror&val=1"
curl "http://192.168.0.3/control?var=aec&val=0"
curl "http://192.168.0.3/control?var=aec_value&val=600"
curl "http://192.168.0.3/control?var=colorbar&val=1"
```

---

## Preference catalog (`var` / `val`)

### Stream / encode

| `var` | `val` | Effect | Also via C++ |
| --- | --- | --- | --- |
| `quality` | `1`–`100` | JPEG quality (lower = faster/smaller) | `stream.setQuality(q)` |
| `frameskip` | `0`+ | Skip N frames between encodes | `stream.setFrameSkip(n)` |
| `framesize` | `0`–`9` | Output size via PPA | `stream.setFramesize(fs)` |

#### `framesize` values

| `val` | Enum | Output |
| :---: | --- | --- |
| `0` | `ESP32P4_STREAM_FHD` | 1920×1072 |
| `1` | `ESP32P4_STREAM_HD` | 1280×720 |
| `2` | `ESP32P4_STREAM_XGA` | 1024×576 |
| `3` | `ESP32P4_STREAM_SVGA` | 800×640 |
| `4` | `ESP32P4_STREAM_VGA` | 640×480 |
| `5` | `ESP32P4_STREAM_HVGA` | 480×320 |
| `6` | `ESP32P4_STREAM_CIF` | 400×288 |
| `7` | `ESP32P4_STREAM_QVGA` | 320×240 |
| `8` | `ESP32P4_STREAM_HQVGA` | 240×176 |
| `9` | `ESP32P4_STREAM_QQVGA` | 160×128 |

```cpp
stream.setFramesize(ESP32P4_STREAM_VGA);
stream.setQuality(40);
stream.setFrameSkip(0);
```

**Tuning tips**

| Goal | Preference |
| --- | --- |
| Max FPS | `quality` ~25–35, `framesize` QVGA/HVGA, `frameskip` 0–1 |
| Sharp stills | `quality` 50–70, `framesize` SVGA |
| Weak Wi‑Fi | smaller `framesize`, lower `quality` |

---

### Geometry

| `var` | `val` | Effect | C++ |
| --- | --- | --- | --- |
| `hmirror` | `0` / `1` | Horizontal mirror | `cam.setHMirror(bool)` |
| `vflip` | `0` / `1` | Vertical flip | `cam.setVFlip(bool)` |

---

### Exposure / gain (OV5647)

| `var` | `val` | Effect | C++ |
| --- | --- | --- | --- |
| `aec` | `0` / `1` | Auto exposure | `cam.setAEC(bool)` |
| `agc` | `0` / `1` | Auto gain | `cam.setAGC(bool)` |
| `aec_value` | `uint16` | Manual exposure lines | `cam.setExposure(u16)` |
| `agc_gain` | `uint16` | Manual gain | `cam.setGain(u16)` |
| `gainceiling` | `uint16` | AGC ceiling | `cam.setGainCeiling(u16)` |

```bash
# Manual exposure path
curl "http://ip/control?var=aec&val=0"
curl "http://ip/control?var=agc&val=0"
curl "http://ip/control?val=700&var=aec_value"
curl "http://ip/control?var=agc_gain&val=8"
```

```cpp
cam.setAEC(false);
cam.setAGC(false);
cam.setExposure(700);
cam.setGain(8);
cam.setGainCeiling(16);
```

---

### Debug

| `var` | Alias | `val` | Effect | C++ |
| --- | --- | --- | --- | --- |
| `colorbar` | `test_pattern` | `0` / `1` | Sensor test pattern | `cam.setTestPattern(bool)` |

```bash
curl "http://ip/control?var=test_pattern&val=1"
```

---

## Camera `esp32p4_cam_config_t` preferences (boot-time)

Set **before** `cam.begin(cfg)` — not via HTTP.

| Field | Preference | Example |
| --- | --- | --- |
| `fb_count` | More buffering / smoother stream | `2` or `3` |
| `frame_size` | Native CSI resolution | `ESP32P4_FRAMESIZE_800X640` |
| `pixel_format` | Sketch framebuffer (default RGB565; not the CSI RAW10 table) | `ESP32P4_PIXFORMAT_RGB565` |
| `sensor` | Force sensor | `ESP32P4_SENSOR_OV5647` |
| `lane_bit_rate_mbps` | CSI lane rate | `200` |
| `ldo_mv` / `ldo_chan` | MIPI PHY power | `2500` / `3` |
| `sda` / `scl` | I²C pins | `7` / `8` |
| `test_pattern` | Start with pattern | `true` |
| `xclk_hz` | Sensor clock | `24000000` |

```cpp
esp32p4_cam_config_t cfg = esp32p4_cam_config_default();
cfg.fb_count = 3;
cfg.sensor = ESP32P4_SENSOR_AUTO;
cfg.frame_size = ESP32P4_FRAMESIZE_800X640;
cam.begin(cfg);
```

Stream resolution at runtime is **independent**: use HTTP/`setFramesize` (PPA), not `frame_size`, for UI scaling.

---

## JavaScript (from UI)

The built-in page calls:

```text
GET /control?var=...&val=...
GET /status
```

and sets `<img src="http://host:81/stream">`.

---

## Debug pipeline (`APP_DEBUG` + NVS)

Library logs for camera / PPA / JPEG / stream / Wi-Fi / audio / H.264 / SD / ISP / net.

Each example should call:

```cpp
#ifndef APP_NAME
#define APP_NAME "04_WiFiMjpeg"
#endif
#ifndef APP_DEBUG
#define APP_DEBUG ESP32P4_DBG_LIVE
#endif
ESP32P4_Debug dbg;
dbg.begin(APP_NAME, APP_DEBUG);
```

`stream.loop()` (or `dbg.poll()`) reads Serial. Saved mask lives in NVS namespace `csi_dbg`.

| Bit | Name | Value |
| --- | --- | :---: |
| cam | `ESP32P4_DBG_CAM` | 1 |
| ppa | `ESP32P4_DBG_PPA` | 2 |
| jpeg | `ESP32P4_DBG_JPEG` | 4 |
| stream | `ESP32P4_DBG_STREAM` | 8 |
| wifi | `ESP32P4_DBG_WIFI` | 16 |
| audio | `ESP32P4_DBG_AUDIO` | 32 |
| h264 | `ESP32P4_DBG_H264` | 64 |
| sd | `ESP32P4_DBG_SD` | 128 |
| isp | `ESP32P4_DBG_ISP` | 256 |
| net | `ESP32P4_DBG_NET` | 512 |
| live | `ESP32P4_DBG_LIVE` | 543 |

Serial: `d` dump, `d=543` set LIVE, `d=r` restore sketch mask.

HTTP (UI port):

```bash
curl "http://<ip>/debug"
curl "http://<ip>/control?var=debug&val=543"
curl "http://<ip>/control?var=debug_ms&val=1000"
```

Log tags: `CSI_E` event, `CSI_D` periodic, `CSI_S` stall. `jpeg send stall` = TCP made no progress for 250 ms after a JPEG body started. `skip jpeg - tcp congested` = frame dropped to keep the session.

---

## Python viewer

```bash
pip install -r examples/04_WiFiMjpeg/requirements.txt
python examples/04_WiFiMjpeg/cam_wifi_viewer.py <ip> 81
```

---

## Full preference cheat sheet

| Layer | How to change | When |
| --- | --- | --- |
| Board / PSRAM | Arduino IDE Tools menu | Compile time |
| CSI pins / FB count | `board_config.h` → `esp32csi_cam_config()` | Before `cam.begin` |
| Mirror / AEC / gain | `cam.set*` or `/control` | Runtime |
| JPEG quality / stream size | `stream.set*` or `/control` (`framesize` 0–9) | Runtime |
| Debug mask | `dbg.begin`, Serial `d=`, `/control?var=debug` | Runtime (NVS `csi_dbg`) |
| Detect / face model | `det.begin(ESP32P4_DET_*)` / `face.begin(ESP32P4_FACE_*)` | After storage mount |

---

## WebPreview (example 43)

`ESP32P4_WebPreview` does **not** run the Camera UI. You `capture()` → `present(fb)`. One HTTP port (default 80):

| Path | Role |
| --- | --- |
| `GET /` | HTML + `<img src="/stream">` |
| `GET /stream` | MJPEG of the last `present()` |
| `GET /jpg` | Last JPEG still |
| `GET /dets` | JSON from `preview.setStatusJson(...)` |

No `/control` sensor sliders. For those use `ESP32P4_MjpegServer`.

← [API Reference](API-Reference.md) · [Home](Home.md) →
