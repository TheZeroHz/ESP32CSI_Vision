# CSI cameras (ESP32CSI_Vision)

First-party MIPI-CSI stack for ESP32-P4 — **no Arduino `ESP_Video`**. Sketches keep using:

```cpp
#include "board_config.h"
ESP32P4_Camera cam;
cam.begin(esp32csi_cam_config());  // GPIOs + sensor from board_config.h
camera_fb_t *fb = cam.capture();   // RGB565 in PSRAM by default
cam.release(fb);
```

Programmer API: [wiki/API-Reference.md](wiki/API-Reference.md) · sensor IDs: [wiki/Enums-and-Types.md](wiki/Enums-and-Types.md).

ESP32-P4 ISP RGB565 pipeline max is **1920×1080**. Higher native sensors use binned/cropped modes.

### Sensor MIPI vs sketch output

These are **not** the same field. Do not copy the table’s RAW10/RAW8 into `cfg.pixel_format`.

| Layer | What it is | Typical OV2710 / OV5647 |
| --- | --- | --- |
| **CSI input** (table column below) | What the sensor sends over MIPI | Bayer **RAW10** (BGGR / GBRG / …) |
| **Sketch output** (`cfg.pixel_format` / `fb->format`) | What `cam.capture()` returns | **`ESP32P4_PIXFORMAT_RGB565`** |

Pipeline: **sensor RAW8/RAW10** → P4 ISP (demosaic + **IPA tables**: BLC / LSC / CCM / AWB / AGC / gamma) → framebuffer (`RGB565` by default). A few sensors (e.g. OV5645) already output RGB565 on CSI; those skip demosaic.

Default remains `cfg.pixel_format = ESP32P4_PIXFORMAT_RGB565` so MJPEG, face, CV, default H.264, and UVC JPEG keep working. `setFormat()` / `cfg.pixel_format` also accept RGB888, YUV422 (UYVY), **YUYV**, YUV420, GRAY8, RAW8, RAW10, and **JPEG** (HW encode of RGB565; `fb->format` is JPEG). RAW capture larger than 1920×1080 bypasses the ISP (P4 ISP max).

**Who needs RGB565**

| Consumer | Framebuffer |
| --- | --- |
| MJPEG UI / stills (unless capture is already JPEG) | RGB565 |
| Face detect / CV / QR | RGB565 |
| H.264 default + MJPEG record | RGB565 |
| USB UVC gadget | JPEG from RGB565 (or JPEG capture) |
| H.264 `encode(fb)` YUV path | YUV420 or YUV422 |

ESP32-P4 has **one MIPI CSI host**. `cfg.csi_id` must be 0; a second CSI `ESP32P4_Camera` fails. **DVP**, **SPI**, and **USB-host UVC** are separate buses (`cfg.bus`) and can run **at the same time** as CSI (`28_DualCam`). Two objects, same `capture()`.

```cpp
cfg.bus = ESP32P4_CAM_BUS_CSI;       // default
cfg.bus = ESP32P4_CAM_BUS_DVP;       // OV2640 parallel
cfg.bus = ESP32P4_CAM_BUS_SPI;       // SP0A39 1-bit gray
cfg.bus = ESP32P4_CAM_BUS_UVC_HOST;  // USB webcam
```

USB **device** gadget (`ESP32P4_Uvc`) and USB **host** UVC cannot share the PHY in one sketch.

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
| **Experimental** | Wired end-to-end; kept for rare field-only sensors |
| **Detect** | Chip-ID / soft detect only — no usable mode yet |

## Sensor matrix

**CSI input** = MIPI format from the sensor (Bayer RAW or sensor RGB565). Capture to the sketch is still RGB565 (see above).

### Tier A — Espressif MIPI parity

| Sensor | SCCB (7-bit) | CSI input | Lanes | Mbps/lane | Bayer / fmt | Status | Source |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SC2336 | 0x30 | 720p / 1080p RAW10 | 2 | 405 | BGGR | Supported | Espressif Apache-2.0 |
| SC2331 | 0x30/0x32 | 1080p RAW10 | 2 | 315 | BGGR | Supported | Espressif Apache-2.0 |
| OV5647 | 0x36 | 800×640 RAW8, 1080p RAW10 | 2 | 200–500 | GBRG | Supported | Existing + Espressif; DW9714 AF @ 0x0C (`afScan`) |
| OV5645 | 0x3C/0x3D | 1280×960 RGB565; VGA / 1080p / 5MP YUV422 | 2 | 416–672 | sensor RGB/YUV | Supported | Espressif Apache-2.0; 5MP skips P4 ISP |
| OV5640 | 0x3C/0x3D | 1280×720 RGB565 | 2 | 320 | sensor RGB565 | Supported | Espressif MIPI only (no DVP) |
| OV2710 | 0x36 | 720p / 1080p RAW10 | 1 | 800 | BGGR | Supported | Espressif Apache-2.0 |
| OV9281 | 0x60/0x10 | 1280×720 RAW8 | 2 | 400 | BGGR (mono) | Supported | Espressif Apache-2.0 |
| SC202CS | 0x36 | 1280×720 RAW8 | 1 | 360 | BGGR | Supported | Espressif Apache-2.0 |
| SC1346 | 0x30 | 720p RAW10 | 1 | 480 | BGGR | Supported | Espressif Apache-2.0 |
| SC030IOT | 0x68 | 640×480 RAW8 | 1 | 240 | BGGR | Supported | Espressif Apache-2.0 |
| SC035HGS | 0x30 | 640×480 RAW8 | 2 | 360 | BGGR | Supported | Espressif Apache-2.0 |
| OS02N10 | 0x3C/0x3D | 720p / 1080p RAW10 | 2 | 480 | BGGR | Supported | Espressif Apache-2.0 |
| OS04C10 | 0x36/0x10 | 960×1280 RAW10 | 1 | 800 | BGGR | Supported | Espressif Apache-2.0 |
| GC2145 | 0x3C | 800×600 RGB565 (VGA / UXGA optional) | 1 | 336 | sensor RGB565 | Supported | Espressif Apache-2.0 |
| STI2250 | 0x37/0x10 | 800×600 RAW8 | 1 | 400 | BGGR | Supported | Espressif Apache-2.0 |
| GC2607 | 0x37 | 1080p RAW10 | 2 | 672 | GRBG | Supported | Espressif Apache-2.0 |
| SC121AT | 0x30 | — | — | — | YUV | Detect | Espressif YUV table is empty (`REG_END` only) |
| MIRA220 | 0x54 | 1024×600 RAW8 | 2 | 400 | BGGR | Supported | Espressif Apache-2.0 |
| LT6911 | 0x2B | HDMI 720p / 1080p YUV422 | 2 | 357–654 | YUYV | Supported | HDMI-CSI bridge; burn LT6911D firmware first |

### ISP IPA tables (Espressif JSON, in-process)

Applied when the sensor is RAW and the P4 ISP is on. Files are Espressif MIT from `esp_cam_sensor` (eco5 where they ship both).

| Sensor | CCM | AWB locus | AE / anti-flicker | LSC mesh | BLC |
| --- | --- | --- | --- | --- | --- |
| SC2336 | 12 CT | yes (10 refs + zones) | target 57, 50 Hz, PWL | 1920×1080, 7 CT | offset 16 |
| SC2331 | 8 CT | yes (6 refs + zones) | target 45, PWL | — | offset 16 |
| SC202CS | 19 CT | range only | target 62, 50 Hz | 1280×720, 9 CT | offset 16 |
| SC1346 | 19 CT | range only | target 62, 50 Hz | 1280×720, 9 CT | offset 16 |
| OS02N10 | 10 CT | range only | yes | 1920×1080, 6 CT | offset 16 |
| OS04C10 | 11 CT | yes (12 refs) | yes, PWL | 960×1280, 7 CT | offset 16 |
| STI2250 (mono) | 1 | — | target 82 | 800×600, 1 CT | offset 16 |
| OV5647 | 1 | generic locus | default 80 (no AGC JSON) | — | offset 16 |
| OV2710 | 15 CT | generic locus | no AGC JSON | — | offset 16 |
| OV9281 (mono) | 1 | — | target 19 | — | offset 16 |
| SC035HGS | 1 | range only | target 62 | — | offset 16 |
| MIRA220 (mono) | 1 | — | target 82 | — | offset 16 |
| GC2607 | 1 | range only | target 62 | — | offset 16 |
| SC030IOT | SC035HGS alias | range only | target 62 | — | SC035HGS BLC |
| IMX708 / IMX219 / IMX477 / IMX462 | — | generic | generic | — | generic offset 16 |
| OV5645 / GC2145 | — | — | — | — | sensor RGB/YUV, no ISP IPA |
| SC121AT | — | — | — | — | detect-only |

### Tier B — market / Pi-class (beyond Espressif)

| Sensor | SCCB | CSI input | Status | Notes |
| --- | --- | --- | --- | --- |
| IMX708 | 0x1A | 720p / 2304×1296 RAW10 | Supported | Pi Cam 3; 2304×1296 RGB565 auto RAW10 (ISP max 1080p) |
| IMX219 | 0x10 | 1080p RAW10 | Supported | Pi Cam v2; analogue gain + coarse exposure |
| IMX477 | 0x1A/0x10 | 1080p / 1332×990 RAW10 | Supported | Pi HQ Cam (+ IMX378). Chip ID `0x0016=0x0477`. 2-lane RAW10 crop to 1920×1080 (ISP max). `FRAMESIZE_HD` → 1332×990. 24 MHz INCK (`cfg.xclk` if the module has no crystal). |
| IMX462 | 0x1A | 1080p / 720p RAW10 | Supported | STARVIS (also IMX290/327). 2-lane RAW10. Typical modules use **37.125 MHz onboard** — leave `cfg.xclk = -1`. `FRAMESIZE_HD` → 720p. |
| IMX335 / IMX415 | varies | — | Detect | Security modules |
| GC2083 / GC2093 | 0x37/0x10 | — | Detect | Common CN MIPI on P4 carriers |

Force a Sony module if AUTO is ambiguous (chip ID still wins for IMX477 / IMX378):

```cpp
esp32p4_cam_config_t cfg = esp32p4_cam_config_board(ESP32P4_BOARD_CUSTOM);
cfg.sensor = ESP32P4_SENSOR_IMX477;  // or ESP32P4_SENSOR_IMX462
cfg.frame_size = ESP32P4_FRAMESIZE_1080P;
cam.begin(cfg);
```

### Tier C — catalog / detect-first

| Sensor | Status | Notes |
| --- | --- | --- |
| OV7251 | Detect | Global-shutter mono |
| IMX296 | Detect | Global-shutter industrial |
| Arducam-IMX500 | Detect | Soft presence @ 0x0C |

### DVP / SPI (not MIPI)

| Sensor | Bus | SCCB | Mode | Status |
| --- | --- | --- | --- | --- |
| OV2640 | DVP 8-bit | 0x30 | 640×480 RGB565 LE ~6 fps | Supported — `24_DvpCam` |
| SP0A39 | SPI 1-bit | 0x21 | 640×480 GRAY8 ~4 fps | Supported — `25_SpiCam` |

USB-host UVC has no SCCB sensor enum; `cfg.bus = ESP32P4_CAM_BUS_UVC_HOST` (`26_UsbHostUvc`).

## vs Arduino `ESP_Video`

| | ESP32CSI_Vision | ESP_Video |
| --- | --- | --- |
| API | Arduino `camera_fb_t` + `ESP32P4_Frame` RAII; optional `ESP32P4_V4l2` | V4L2 device nodes |
| ISP | Espressif IPA tables in-process (BLC / LSC / CCM / AWB / AGC) | IDF IPA behind V4L2 (not exposed in Arduino) |
| Formats | RGB565/888, YUV422/420, GRAY8, RAW8/10, **JPEG capture** | Same family via `setFormat`; JPEG via M2M `/dev/video10` |
| H.264 | RGB565 default; YUV420 packed / UYVY native; runtime bitrate/GOP/QP | `/dev/video11` YUV M2M |
| Apps | MJPEG / face / H.264 / CV / **UVC webcam** share one FB path | App must bridge V4L2 |
| USB | Device gadget (`ESP32P4_Uvc`) **and** host UVC (`cfg.bus = UVC_HOST`) | No Arduino UVC gadget (IDF example only); host via `/dev/video40` |
| Buses | CSI (default) + DVP + SPI + host UVC, all `camera_fb_t` | CSI + DVP + SPI + host UVC (V4L2 nodes) |
| V4L2 | Opt-in POSIX `/dev/video0` + M2M `/dev/video10–12` + ISP `/dev/video20` (`27_V4l2Ctl`). `v4l.mmap()` zero-copy. | Native `/dev/video*` + kernel mmap |
| Sensors | Ops registry + AUTO probe (IMX708 / IMX219 / **IMX477** / **IMX462**) | Depends on esp_cam_sensor + video stack |
| Dependency | None on `ESP_Video` (do not init it on the same CSI host) | Required |

## License

Espressif register tables under `src/cam/sensors/*/` keep **Apache-2.0** SPDX headers. Do not link `ESP_Video`. IMX477 / IMX462 init data follows the public Linux register maps (`imx477.c` / `imx290.c`), rewritten here for 2-lane RAW10 and P4 ISP 1920×1080 — the GPL drivers themselves are not vendored.
