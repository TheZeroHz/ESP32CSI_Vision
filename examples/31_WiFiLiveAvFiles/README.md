# 31_WiFiLiveAvFiles

Wi-Fi live video + browser audio + FileMgr.

Same features as `30_EthLiveAvFiles`, over ESP32-C6 SDIO Wi-Fi instead of Ethernet.

**Wiring:** [`board_config.h`](board_config.h) in this folder — [Custom-Boards.md](../../docs/Custom-Boards.md)

## Config-first setup

```cpp
#include "board_config.h"

esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
esp32p4_sd_config_t sd_cfg = esp32csi_sd_config();
esp32p4_mic_config_t mic_cfg = esp32csi_mic_config();
esp32csi_wifi_config_t wifi_cfg = esp32csi_wifi_config();
```

Edit `CFG_WIFI_SSID` / `CFG_WIFI_PASS` in `board_config.h`. If STA join fails, the sketch falls back to SoftAP.

## Ports

| Service | Port |
| --- | --- |
| Camera UI | 80 |
| MJPEG `/stream` | 81 |
| File manager UI | 82 |
| File transfers | 83 |
| Live PCM `/audio.pcm` | 84 |

## Setup

1. Arduino-ESP32 **3.3.x**, board **ESP32P4 Dev Module**, **PSRAM Enabled**, large app partition.
2. Insert a **FAT32** TF card (required here because the example uses explicit SD config).
3. Upload, Serial @ 115200, open the printed IP.
4. Record tab has the live audio player; Capture writes `/IMG/*.jpg`; Record writes `/VIDEO/*.mp4` with mic audio.

## Debug lag / freeze

Default `APP_DEBUG` is `ESP32P4_DBG_LIVE`. Serial @ 115200:

- `d` — dump mask and last stall
- `d=543` — live components (cam+ppa+jpeg+stream+wifi+net)
- `d=r` — restore the sketch default

Browser: `http://<ip>/debug`. Look for `CSI_S ... jpeg send stall`, `PPA`, `capture stall`, `STA lost`. Hard-refresh the UI after flashing (Ctrl+F5).
