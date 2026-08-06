# ESP32CSI_Vision Wiki

**Library:** [ESP32CSI_Vision](https://github.com/thezerohz/ESP32CSI_Vision) · **Author:** [Rakib Hasan](https://github.com/thezerohz) (@thezerohz)  
**Version:** 3.1.0 · **Arduino IDE:** supported · **Backend today:** ESP32-P4 MIPI CSI

Welcome. This wiki documents every public API, config preference, HTTP control, and usage example.

---

## Start here

| Page | Contents |
| --- | --- |
| [Getting Started](Getting-Started.md) | Install, board settings, first sketch |
| [**API Reference (full)**](API-Reference.md) | All classes, params, returns, examples |
| [HTTP & Preferences](HTTP-and-Preferences.md) | MJPEG ports, `/control`, `/status`, live knobs |
| [Enums & Types](Enums-and-Types.md) | Boards, sensors, framesizes, structs |
| [Examples Map](Examples-Map.md) | What each sketch teaches |

---

## Module map

```text
#include <ESP32CSI_Vision.h>
        │
        ├── ESP32P4_Camera      CSI capture + sensor prefs
        ├── ESP32P4_Jpeg        HW encode / decode
        ├── ESP32P4_Ppa         Scale / rotate / mirror
        ├── ESP32P4_Img         Pixel helpers
        ├── ESP32P4_Dsp         Motion detect
        ├── ESP32P4_MjpegServer Wi‑Fi UI + stream
        ├── ESP32P4_WhoPipeline Async frame queue
        ├── esp32p4_psram_*     PSRAM alloc
        └── ESP32P4_FaceDetect  ESP-DL faces (ESP-IDF only)
```

---

## Quick links

- [README](../../README.md)
- [Changelog](../../CHANGELOG.md)
- [Face detect IDF example](../../idf_examples/08_FaceDetect/README.md)

---

## Convention

| Symbol | Meaning |
| --- | --- |
| `bool` return | `true` = success, `false` = fail |
| `camera_fb_t *` | Must `release()` after use |
| PSRAM | Required for multi-FB + JPEG buffers |
| Arduino IDE | Capture / JPEG / MJPEG / DSP / WHO |
| ESP-IDF | Required for `ESP32P4_FaceDetect` |
