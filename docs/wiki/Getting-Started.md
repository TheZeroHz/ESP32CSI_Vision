# Getting Started

## 1. Install (Arduino IDE)

1. Clone or download [ESP32CSI_Vision](https://github.com/thezerohz/ESP32CSI_Vision).
2. Copy to `Documents/Arduino/libraries/ESP32CSI_Vision`, **or** Sketch → Include Library → Add .ZIP Library.
3. Restart Arduino IDE.

Every example has a `board_config.h` tab next to the `.ino`. That file includes the library:

```cpp
#include "board_config.h"   // includes <ESP32CSI_Vision.h>
```

Your own sketch: copy `examples/00_BoardConfig/board_config.h` beside the `.ino`.

### PlatformIO

```ini
lib_deps = https://github.com/thezerohz/ESP32CSI_Vision.git
```

ESP-IDF components: `idf_examples/` (face / COCO / Ethernet face web).

---

## 2. Board menu (ESP32-P4)

| Setting | Recommended |
| --- | --- |
| Board | ESP32P4 Dev Module (or your vendor’s P4 board) |
| **PSRAM** | **Enabled** |
| Flash | 16 MB if the module has it |
| Partition | `app3M_fat9M_16MB` if you store `.espdl` / video on FFat |
| USB | USB-OTG (TinyUSB). For `23_UsbUvc`: CDC On Boot **Disabled** (Serial on UART) |
| Upload | 921600 (lower if flaky) |

```text
esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB
```

---

## 3. Wire the board

Pins are **not** in `src/`. Edit **`board_config.h` in the example folder** from your schematic.

Guide: [Custom-Boards.md](../Custom-Boards.md)

1. Uncomment one sensor / bus / framesize / SD / mic / Wi-Fi / ETH block.
2. Flash **`00_BoardConfig`**.
3. Read Serial `CFG:` lines. Fix GPIOs until they match the PCB.

---

## 4. First sketch

**File → Examples → ESP32CSI_Vision → 01_CamTest**

```cpp
#include "board_config.h"

ESP32P4_Camera cam;
ESP32P4_Debug dbg;

void setup() {
  Serial.begin(115200);
  dbg.begin("01_CamTest", ESP32P4_DBG_CAM);

  esp32p4_cam_config_t cfg = esp32csi_cam_config();
  esp32csi_print_cam_config(cfg);
  if (!cam.begin(cfg)) {
    Serial.println("cam.begin failed");
    while (true) delay(1000);
  }
}

void loop() {
  dbg.poll();
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) return;
  Serial.printf("%ux%u %s %u bytes\n", fb->width, fb->height, cam.formatName(),
                (unsigned)fb->len);
  cam.release(fb);
}
```

Serial @ **115200**. Expect `CSI: streaming … RGB565`. Always `cam.release(fb)`.

### What capture returns

`cam.capture()` defaults to **RGB565** (`fb->len = width * height * 2`). Sensor tables that say RAW10 (OV5647, OV2710, …) mean the **MIPI packet**; the ISP converts unless you `cam.setFormat(...)` to RAW / YUV / RGB888 / JPEG.

| Consumer | Needs |
| --- | --- |
| MJPEG UI, CV, QR, Face, default H.264, UVC JPEG | RGB565 (or JPEG in for UVC/MJPEG pass-through) |
| H.264 | RGB565, or YUV420 / YUV422 / YUYV without an RGB convert |

RAW CSI uses Espressif IPA (BLC / CCM / LSC / AWB / AGC) when a JSON table exists. Serial may show `IPA SC2336 LSC BLC` or `IPA OV5647 BLC`. IMX708 / IMX219 / IMX477 / IMX462 stay on the generic 3A path. Frames larger than 1920×1080 on RAW sensors capture as RAW10 (ISP max). OV5647 AF: `cam.afScan()` after `begin()`.

ESP32-P4 has **one** MIPI CSI host (`csi_id` = 0). DVP / SPI / USB-host UVC use `cfg.bus` (examples 24–26).

---

## 5. Next sketches

| Goal | Sketch |
| --- | --- |
| Wi-Fi MJPEG UI | `04_WiFiMjpeg` |
| Ethernet live AV + files | `30_EthLiveAvFiles` |
| microSD | `09_SdCard` |
| Mic → WAV | `15_MicSdRecord` |
| Video + mic MP4 (no stream) | `44_AvSdRecord` |
| Detect JSON (Serial) | `42_DetectApi` |
| You own the FB + browser | `43_CamWebModels` |
| USB gadget webcam | `23_UsbUvc` |
| POSIX `/dev/video0` | `27_V4l2Ctl` |

HTTP knobs: [HTTP & Preferences](HTTP-and-Preferences.md)  
Methods: [API Reference](API-Reference.md)  
All sketches: [Examples Map](Examples-Map.md)

---

## Include aliases

```cpp
#include <ESP32CSI_Vision.h>   // preferred (or board_config.h)
#include <ESP32P4_Vision.h>    // alias
#include <ESP32P4_Cam.h>       // alias
```
