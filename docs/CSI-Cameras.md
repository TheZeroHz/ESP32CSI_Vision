# CSI cameras (ESP32CSI_Vision)

First-party MIPI-CSI stack for ESP32-P4 — **no Arduino `ESP_Video` / V4L2**. Sketches keep using:

```cpp
ESP32P4_Camera cam;
cam.begin(ESP32P4_BOARD_…);       // AUTO probe by default
camera_fb_t *fb = cam.capture();  // RGB565 in PSRAM
```

ESP32-P4 ISP RGB565 pipeline max is **1920×1080**. Higher native sensors use binned/cropped modes.

## Boards

| Board enum | Typical sensor | SCCB (default) | Notes |
| --- | --- | --- | --- |
| `ESP32P4_BOARD_GUITION_M3` | OV5647 / IMX708 | SDA7 / SCL8, LDO3 @ 2.5 V | Guition JC-ESP32P4-M3 |
| `ESP32P4_BOARD_FUNCTION_EV` | SC2336 | SDA7 / SCL8 | Espressif Function-EV CSI |
| `ESP32P4_BOARD_WAVESHARE_NANO` | OV5647 | SDA7 / SCL8 | Waveshare Nano CSI |
| `ESP32P4_BOARD_CUSTOM` | any | fill `esp32p4_cam_config_t` | Full pin / LDO control |

Default `sensor` is `ESP32P4_SENSOR_AUTO` (ordered registry probe).

## Status legend

| Status | Meaning |
| --- | --- |
| **Supported** | Detect + mode table + CSI/ISP → RGB565 |
| **Experimental** | Wired end-to-end; needs more field validation |
| **Detect** | Chip-ID / soft detect only — no usable mode yet |

## Sensor matrix

### Tier A — Espressif MIPI parity

| Sensor | SCCB (7-bit) | Mode(s) | Lanes | Mbps/lane | Bayer / fmt | Status | Source |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SC2336 | 0x30 | 720p / 1080p RAW10 | 2 | 405 | BGGR | Supported | Espressif Apache-2.0 |
| OV5647 | 0x36 | 800×640 RAW8, 1080p RAW10 | 2 | 200–500 | GBRG | Supported | Existing + Espressif |
| OV5645 | 0x3C/0x3D | 1280×960 RGB565 | 2 | 448 | sensor RGB565 | Experimental | Espressif Apache-2.0 |
| OV2710 | 0x36 | 1080p RAW10 | 1 | 800 | BGGR | Experimental (T-Display P4) | Espressif Apache-2.0 |
| OV9281 | 0x60/0x10 | 1280×720 RAW8 | 2 | 400 | BGGR (mono) | Supported | Espressif Apache-2.0 |
| SC202CS | 0x36 | 1280×720 RAW8 | 1 | 360 | BGGR | Supported | Espressif Apache-2.0 |
| SC1346 | 0x30 | 720p RAW10 | 1 | 480 | BGGR | Supported | Espressif Apache-2.0 |
| SC030IOT | 0x68 | 640×480 RAW8 | 1 | 240 | BGGR | Experimental | Espressif Apache-2.0 |
| SC035HGS | 0x30 | 640×480 RAW8 | 2 | 360 | BGGR | Supported | Espressif Apache-2.0 |
| OS02N10 | 0x3C/0x3D | 1080p RAW10 | 2 | 480 | BGGR | Experimental | Espressif Apache-2.0 |
| OS04C10 | 0x36/0x10 | 960×1280 RAW10 | 1 | 800 | BGGR | Experimental | Espressif Apache-2.0 |
| GC2145 | 0x3C | — | — | — | — | Detect | Espressif tables present |
| STI2250 | 0x37/0x10 | 800×600 RAW8 | 1 | 400 | BGGR | Experimental | Espressif Apache-2.0 |
| SC121AT | — | — | — | — | YUV | Detect | Espressif Apache-2.0 |
| MIRA220 | 0x54 | 1024×600 RAW8 | 2 | 400 | BGGR | Experimental | Espressif Apache-2.0 |

### Tier B — market / Pi-class (beyond Espressif)

| Sensor | SCCB | Mode(s) | Status | Notes |
| --- | --- | --- | --- | --- |
| IMX708 | 0x1A | 720p / 2304×1296 RAW10 | Supported | Pi Cam 3 |
| IMX219 | 0x10 | 1080p RAW10 | Experimental | Pi Cam v2 modules |
| IMX477 | 0x1A/0x10 | — | Detect | HQ Cam; full-res RAW later (license-clean tables) |
| IMX335 / IMX415 | varies | — | Detect | Security modules |
| GC2083 / GC2093 | 0x37/0x10 | — | Detect | Common CN MIPI on P4 carriers |

### Tier C — catalog / detect-first

| Sensor | Status | Notes |
| --- | --- | --- |
| OV7251 | Detect | Global-shutter mono |
| IMX296 / IMX462 | Detect | Industrial / low-light modules |
| Arducam-IMX500 | Detect | Soft presence @ 0x0C |

## vs Arduino `ESP_Video`

| | ESP32CSI_Vision | ESP_Video |
| --- | --- | --- |
| API | Arduino `camera_fb_t` RGB565 | V4L2 device nodes |
| Apps | MJPEG / face / H.264 / CV share one FB path | App must bridge V4L2 |
| Sensors | Ops registry + AUTO probe | Depends on esp_cam_sensor + video stack |
| Dependency | None on `ESP_Video` | Required |

## License

Espressif register tables under `src/cam/sensors/*/` keep **Apache-2.0** SPDX headers. Do not link `ESP_Video`. Raspberry Pi Linux GPL tables are **not** vendored; Pi sensors use original or license-clean init data only.
