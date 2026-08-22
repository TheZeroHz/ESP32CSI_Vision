# Arduino examples

These sketches are **board-agnostic**. Each folder has its own
[`board_config.h`](00_BoardConfig/board_config.h) (Arduino IDE: a tab next to the `.ino`).
Pins are **not** in `src/`.

| Doc | Contents |
| --- | --- |
| [Custom boards](../docs/Custom-Boards.md) | Every `CFG_*` pin / sensor / PHY |
| [Examples Map](../docs/wiki/Examples-Map.md) | What each sketch teaches + snippets |
| [API Reference](../docs/wiki/API-Reference.md) | Classes and methods |
| [Getting Started](../docs/wiki/Getting-Started.md) | Install + first capture |

**Start:** flash **`00_BoardConfig`**, read Serial `CFG:` lines, then:

| Sketch | Use it to verify |
| --- | --- |
| `00_BoardConfig` | Pin dump + camera probe |
| `01_CamTest` | CSI `capture` / `release` |
| `04_WiFiMjpeg` | C6 Wi-Fi + MJPEG UI `:80` / `:81` |
| `09_SdCard` | SDMMC |
| `15_MicSdRecord` | ES8311 → WAV |
| `44_AvSdRecord` | CSI + mic → MP4 (no stream) |
| `24` / `25` / `26` | DVP / SPI / USB-host UVC |
| `30_EthLiveAvFiles` | RMII Ethernet + live AV |
| `42_DetectApi` | `det.infer(fb)` → JSON |
| `43_CamWebModels` | You `capture()` → model → `present(fb)` |

Copy `models/espdl/p4/*.espdl` to `/models/p4/` on SD or FFat before detect sketches.

Do not copy another vendor’s pin numbers onto a different PCB. Use the schematic.
