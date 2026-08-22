# 29_WiFiMjpegWaveshareNano

Waveshare **ESP32-P4-Nano** Wi-Fi camera stream with onboard mic audio.
This is the library's combined **video + audio** example for the Nano.

## Board

| Item | Value |
| --- | --- |
| Camera | OV5647 on MIPI CSI (`ESP32P4_BOARD_WAVESHARE_NANO`, SDA7/SCL8) |
| Wi-Fi | ESP32-C6 over SDIO (CLK18 CMD19 D0–D3 = 14–17, RST54) |
| Mic | ES8311 — I2S DIN **GPIO11**, PA **GPIO53** (differs from Guition M3) |
| Storage | TF slot (SDMMC 43/44/39–42, LDO ch4) |

## Config-first setup

This sketch intentionally uses the helper configs for all three peripherals:

```cpp
esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
esp32p4_sd_config_t sd_cfg = esp32csi_sd_config();
esp32p4_mic_config_t mic_cfg = esp32csi_mic_config();
```

This sketch’s `board_config.h` sets `ESP32CSI_BOARD` to Waveshare and `CFG_MIC_DIN` / `CFG_MIC_PA` (11 / 53). See [Custom-Boards.md](../../docs/Custom-Boards.md).

Typical pin overrides:

```cpp
sd_cfg.clk = 43; sd_cfg.cmd = 44;
sd_cfg.d0 = 39; sd_cfg.d1 = 40; sd_cfg.d2 = 41; sd_cfg.d3 = 42;

mic_cfg.type = ESP32P4_MIC_ES8311;
mic_cfg.i2c_sda = 7; mic_cfg.i2c_scl = 8;
mic_cfg.i2s_mclk = 13; mic_cfg.i2s_bclk = 12; mic_cfg.i2s_ws = 10;
mic_cfg.i2s_dout = 9; mic_cfg.i2s_din = 11; mic_cfg.pa_gpio = 53;
mic_cfg.sample_rate = 16000;
```

## Audio in the “stream”

MJPEG (`/stream`) is **video only**. The example still brings up video and audio together in one UI session:

1. **Live waveform** — open `http://<ip>/` in a browser; the Mic slider drives the ES8311 and the UI polls `/audio` for levels.
2. **Recorded MP4** — tap **Record** / **Stop** in the UI; H.264 video + PCM mic are muxed to `/VIDEO/VID_xxxxx.mp4` on the TF card.

There is no live speaker playback in the MJPEG URL itself (that would need a separate audio stream protocol).

## Setup

1. Arduino-ESP32 **3.3.x**, board **ESP32P4 Dev Module**, **PSRAM Enabled**, large app partition (16 MB flash).
2. Edit `CFG_WIFI_SSID` / `CFG_WIFI_PASS` in `board_config.h`.
3. Insert a **FAT32** TF card. This example intentionally uses explicit `ESP32P4_Sd` config, so SD is required here for stills (`/IMG`) and video (`/VIDEO`).
4. Upload, open Serial @ 115200, browse to the printed IP.

## Viewer

```bash
python examples/04_WiFiMjpeg/cam_wifi_viewer.py <ip> 81
```

The Python viewer shows video only; use the web UI for mic waveform and recording.
