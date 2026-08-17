# HTTP API & Preferences

Live controls for `ESP32P4_MjpegServer` (example `04_WiFiMjpeg`).

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
  "framesize": 0,
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
| `framesize` | Stream enum 0–4 (PPA output) |
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
curl "http://192.168.0.3/control?var=framesize&val=3"
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
| `framesize` | `0`–`4` | Output size via PPA | `stream.setFramesize(fs)` |

#### `framesize` values

| `val` | Enum | Output |
| :---: | --- | --- |
| `0` | `ESP32P4_STREAM_SVGA` | 800×640 |
| `1` | `ESP32P4_STREAM_VGA` | 640×480 |
| `2` | `ESP32P4_STREAM_HVGA` | 480×320 |
| `3` | `ESP32P4_STREAM_QVGA` | 320×240 |
| `4` | `ESP32P4_STREAM_QQVGA` | 160×120 |

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
| `pixel_format` | Sketch framebuffer (keep RGB565; not the CSI RAW10 table) | `ESP32P4_PIXFORMAT_RGB565` |
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
| CSI pins / FB count | `esp32p4_cam_config_t` | Before `cam.begin` |
| Mirror / AEC / gain | `cam.set*` or `/control` | Runtime |
| JPEG quality / stream size | `stream.set*` or `/control` | Runtime |
| Face model | `face.begin(Model)` | ESP-IDF only |

← [API Reference](API-Reference.md) · [Home](Home.md) →
