# Custom board config (any ESP32-P4 vendor)

Wiki: [Getting Started](wiki/Getting-Started.md) · [API Reference](wiki/API-Reference.md) · [Examples Map](wiki/Examples-Map.md)

This library does **not** require a Guition board. Guition JC-ESP32P4-M3, Waveshare P4-Nano, Espressif Function-EV, LilyGO, and unnamed OEM carriers all work if you set **your** GPIOs.

**Config lives next to the example, not in `src/`.** Open any sketch in Arduino IDE and edit the `board_config.h` tab. Each example has its own copy, so changing pins for `04_WiFiMjpeg` does not change `01_CamTest`.

Pins that differ between vendors (always check the schematic):

| Block | Typical differences |
| --- | --- |
| Camera | Bus (CSI / DVP / SPI / USB-host UVC), sensor, SCCB `SDA`/`SCL`, `Wire` vs `Wire1`, XCLK, PWDN/RESET, MIPI LDO |
| microSD | SDMMC `CLK`/`CMD`/`D0–D3`, 1-bit vs 4-bit, SD LDO, bus frequency |
| Mic | ES8311 I2C + I2S `MCLK`/`BCLK`/`WS`/`DOUT`/`DIN`, PA GPIO |
| Wi-Fi | ESP32-C6 SDIO `CLK`/`CMD`/`D0–D3`/`RST` (or no C6 at all) |
| Ethernet | RMII `MDC`/`MDIO`/`POWER`, PHY address, PHY type (IP101 / RTL8201 / …) |

---

## How config works (one rule)

1. **Edit** `board_config.h` in the example folder you are flashing (Arduino: it is a sketch tab). Every camera model, framesize, pixel format, SD, mic, C6 Wi-Fi, and Ethernet PHY option is listed there — uncomment **one** value in each pick-one block.
2. The `.ino` includes that file, then calls helpers. Pins are **not** hidden inside `cam.begin(SOME_BOARD)`:

```cpp
#include "board_config.h"   // includes <ESP32CSI_Vision.h>

esp32p4_cam_config_t cam_cfg = esp32csi_cam_config();
esp32p4_sd_config_t  sd_cfg  = esp32csi_sd_config();
esp32p4_mic_config_t mic_cfg = esp32csi_mic_config();
esp32csi_wifi_config_t wifi_cfg = esp32csi_wifi_config();
esp32csi_eth_config_t  eth_cfg  = esp32csi_eth_config();

esp32csi_print_board();   // Serial dump of every GPIO
cam.begin(cam_cfg);
sd.begin(sd_cfg);
mic.begin(mic_cfg);
esp32csi_wifi_begin(wifi_cfg);
esp32csi_eth_begin(eth_cfg);
```

3. Serial lines starting with `CFG:` are the resolved wiring. If they do not match your schematic, fix **that sketch’s** `board_config.h` and reflash.

`ESP32CSI_BOARD` is only a **label** (Serial + a few library presets). Real GPIOs always come from the `CFG_*` defines in `board_config.h`.

---

## Step-by-step for a new company board

1. Open the vendor schematic / pin table.
2. Copy `examples/00_BoardConfig` (or any example) and open `board_config.h`.
3. Uncomment **one** sensor (or leave `ESP32P4_SENSOR_AUTO` to probe), set CSI/DVP/SPI/UVC pins, SD, mic, Wi-Fi SSID, Ethernet PHY.

```cpp
#define ESP32CSI_BOARD ESP32P4_BOARD_CUSTOM

#define CFG_CAM_BUS     ESP32P4_CAM_BUS_CSI
#define CFG_CAM_SENSOR  ESP32P4_SENSOR_AUTO   // or OV5647 / IMX477 / …
#define CFG_CAM_SDA 7
#define CFG_CAM_SCL 8

#define CFG_SD_CLK 43
#define CFG_SD_CMD 44
#define CFG_SD_D0  39

#define CFG_MIC_DIN 48
#define CFG_MIC_PA  11

#define CFG_WIFI_SSID   "YourSSID"
#define CFG_WIFI_PASS   "YourPassword"
#define CFG_WIFI_C6_CLK 18
#define CFG_WIFI_C6_RST 54

#define ETH_PHY_TYPE ETH_PHY_IP101
#define CFG_ETH_MDC   31
#define CFG_ETH_MDIO  52
#define CFG_ETH_POWER 51
#define CFG_ETH_ADDR  1
```

4. Flash **`00_BoardConfig`**. Confirm the `CFG:` dump. Camera begin is optional in that sketch.
5. Copy the same `board_config.h` into the example you actually want, or edit that example’s file the same way.
6. If a peripheral does not exist on your PCB, skip that example (no C6 → skip Wi-Fi sketches; no ETH PHY → skip `13` / `30` / `32`–`41`).

---

## Camera models (all listed in `board_config.h`)

Uncomment **one** `CFG_CAM_SENSOR`. `AUTO` probes SCCB.

| Group | Enums |
| --- | --- |
| Probe | `ESP32P4_SENSOR_AUTO` |
| Espressif MIPI | `SC2336` `OV5647` `OV5645` `OV2710` `OV9281` `SC202CS` `SC1346` `SC030IOT` `SC035HGS` `OS02N10` `OS04C10` `GC2145` `STI2250` `SC121AT` `MIRA220` `SC2331` `GC2607` `OV5640` `LT6911` |
| Sony / Pi-class MIPI | `IMX708` `IMX219` `IMX477` `IMX462` `IMX335` `IMX415` `IMX296` `GC2083` `GC2093` `OV7251` `ARDUCAM_IMX500` |
| Other buses | `OV2640` (DVP) `SP0A39` (SPI) |

Also pick **one** bus (`CSI` / `DVP` / `SPI` / `UVC_HOST`), framesize, and pixel format. DVP data lanes `D0–D15`, SPI `CS/SCLK/D0–D3`, and USB-host UVC `VID/PID/size` are in the same file.

---

## Optional named presets

Use these **only** if your PCB really matches that product. Still verify Serial `CFG:` lines.

| `ESP32CSI_BOARD` | Typical camera | Notes |
| --- | --- | --- |
| `ESP32P4_BOARD_CUSTOM` | whatever you wired | **Default.** You own every GPIO. |
| `ESP32P4_BOARD_GUITION_M3` | OV5647 / IMX708 | Example `CFG_*` numbers match this product. |
| `ESP32P4_BOARD_WAVESHARE_NANO` | OV5647 | Mic **DIN=11**, **PA=53** — see example `29`. |
| `ESP32P4_BOARD_FUNCTION_EV` | SC2336 | CSI I2C often still 7/8; confirm EV board docs. |

Second I2C bus (LilyGO-style camera on `Wire1`):

```cpp
#define CFG_CAM_WIRE Wire1
#define CFG_CAM_SDA  20
#define CFG_CAM_SCL  21
```

---

## Per-peripheral examples (what to flash)

| You want to verify | Example | What you edit in **that folder’s** `board_config.h` |
| --- | --- | --- |
| All GPIOs printed | `00_BoardConfig` | everything |
| CSI camera only | `01_CamTest` | `CFG_CAM_*` |
| DVP / SPI / USB-host UVC | `24` / `25` / `26` | bus + `CFG_DVP_*` / `CFG_SPI_*` / `CFG_UVC_*` |
| Wi-Fi + MJPEG UI | `04_WiFiMjpeg` | camera + `CFG_WIFI_*` |
| microSD | `09_SdCard` | `CFG_SD_*` |
| ES8311 mic + WAV | `15_MicSdRecord` | `CFG_MIC_*` + SD |
| Ethernet + live AV | `30_EthLiveAvFiles` | camera + SD + mic + `CFG_ETH_*` / `ETH_PHY_TYPE` |
| You pass `camera_fb_t` + model JSON | `43_CamWebModels` | camera + Wi-Fi + model file |

---

## Ethernet PHY type

`board_config.h` sets `ETH_PHY_*` **before** it includes `<ETH.h>`. Pick **one** `ETH_PHY_TYPE`:

```cpp
#define ETH_PHY_TYPE ETH_PHY_IP101    // Guition M3
// #define ETH_PHY_TYPE ETH_PHY_RTL8201
// #define ETH_PHY_TYPE ETH_PHY_LAN8720
// #define ETH_PHY_TYPE ETH_PHY_DP83848
// #define ETH_PHY_TYPE ETH_PHY_KSZ8041
// #define ETH_PHY_TYPE ETH_PHY_KSZ8081
// #define ETH_PHY_TYPE ETH_PHY_DM9051
// #define ETH_PHY_TYPE ETH_PHY_W5500
```

MDC / MDIO / PHY power / address are `CFG_ETH_*`. ESP32-P4 RMII **data** pins are Arduino-ESP32 EMAC defaults (TXD0=34, TXD1=35, TX_EN=49, RXD0=30, RXD1=29, CRS_DV=28, REF_CLK=50) — they are documented in the header, not remapped.

---

## Wi-Fi without a C6

`esp32csi_wifi_begin()` calls `WiFi.setPins(...)` for SDIO-C6. Boards that use a different radio must not use that helper — bring Wi-Fi up yourself, then call `stream.begin(&cam, 80, 35)` as in `04`.

---

## Checklist when capture / SD / mic / net fails

1. Serial `CFG: cam sda=…` matches the schematic (`board_config.h` in **this** example).
2. CSI ribbon seated; LDO channel actually powers the MIPI PHY on **your** module.
3. SD: FAT32, `clk/cmd/d0` not swapped; try `CFG_SD_1BIT 1`.
4. Mic: `DIN` is the data **from** the codec (Waveshare ≠ Guition).
5. Wi-Fi: C6 RST and SDIO data pins; SSID in `board_config.h`.
6. ETH: MDC/MDIO/power, PHY address, `ETH_PHY_TYPE`.
