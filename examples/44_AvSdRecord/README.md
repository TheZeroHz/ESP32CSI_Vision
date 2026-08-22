# 44_AvSdRecord

CSI camera + ES8311 mic → one `.mp4` on SD (or FFat). **No Wi-Fi, Ethernet, or MJPEG stream.**

Pull the file off the card (`/VIDEO/VID_00001.mp4`) and play it on a PC. Audio is AAC-LC muxed on `closeFile()`.

## Setup

1. Edit `board_config.h` in this folder (camera, SDMMC, ES8311). Guide: [Custom-Boards.md](../../docs/Custom-Boards.md).
2. Flash **`00_BoardConfig`** and check Serial `CFG:` lines.
3. Flash this sketch. Serial @ 115200.
4. It records ~10 s automatically, then prints the path.

Change length / size at the top of the `.ino`: `RECORD_MS`, `ENC_W`, `ENC_H`, `RECORD_BITRATE`.

Video-only sibling: `11_H264SdRecord`. Live UI record: `17` / `30` / `31`.
