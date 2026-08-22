# 22_EthQrWeb

Ethernet MJPEG UI with **zxing-cpp** barcode / QR scan and **Smart AE** for ESP32-P4 CSI.

## Pipeline

1. CSI RGB565 → MJPEG stream (JPEG worker never blocks on decode)
2. Frame hook: overlay last result; PPA half-res **GRAY8** snap when idle
3. Low-priority QR task runs `scanGray()` (~1–2 Hz, latest only — no backlog)
4. Overlay drawn on every stream frame (smooth preview)

Sources: [zxing-cpp](https://github.com/zxing-cpp/zxing-cpp) **v2.3.0** under `src/qr/zxing/` (Apache-2.0).

## Smart AE

Enabled after `stream.begin()` only when IPA AGC has no ISP stats (`stream.enableSmartAe(!cam.ispReady())`). RAW CSI uses IPA AGC by default; the SENSOR tab can still force Smart AE.

## Format toggles + persistence

QR sidebar checkboxes enable/disable each symbology. Settings save to:

- Preferred volume (`APP_STORAGE` / AUTO): `/qr/settings.txt` on SD → FFat → LittleFS → SPIFFS
- NVS backup namespace `p4qr` if flash filesystem is unavailable

Decoded **type name** is shown in the Type field and status line.

## Supported formats

| Family | Formats |
| --- | --- |
| Matrix | QR, Micro QR, rMQR, Aztec, PDF417 |
| 1D | Code 128 / 39 / 93, Codabar, EAN-8 / 13, UPC-A / E, ITF |
| GS1 | DataBar, DataBar Expanded, DataBar Limited |

Codabar start/stop (`A`–`D`) are stripped from the payload.

## Flash

Board: ESP32P4 Dev Module · PSRAM On · `app3M_fat9M_16MB`

```text
examples/22_EthQrWeb
```

Open `http://<board-ip>/` → **QR** → SCAN on → toggle formats → hold a code steady in frame.
