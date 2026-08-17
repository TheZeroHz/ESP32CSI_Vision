# Getting Started

## Install (Arduino IDE)

1. Clone or download [ESP32CSI_Vision](https://github.com/thezerohz/ESP32CSI_Vision).
2. Copy to `Documents/Arduino/libraries/ESP32CSI_Vision`  
   **or** Sketch → Include Library → Add .ZIP Library…
3. Restart Arduino IDE.
4. Include:

```cpp
#include <ESP32CSI_Vision.h>
```

### PlatformIO

```ini
lib_deps = https://github.com/thezerohz/ESP32CSI_Vision.git
```

---

## Board settings (ESP32-P4)

| Preference | Recommended |
| --- | --- |
| Board | ESP32P4 Dev Module (or vendor board) |
| **PSRAM** | **Enabled** |
| Flash Size | 16MB (if your module has 16MB) |
| USB / CDC | Match your cable (CH340 vs native USB) |
| Upload speed | 921600 (lower if unstable) |

FQBN example:

```text
esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,USBMode=default,CDCOnBoot=default
```

---

## First sketch

**File → Examples → ESP32CSI_Vision → 01_CamTest**

```cpp
#include <ESP32CSI_Vision.h>

ESP32P4_Camera cam;

void setup() {
  Serial.begin(115200);
  delay(1200);
  if (!cam.begin(ESP32P4_BOARD_GUITION_M3)) {
    Serial.println("camera begin FAILED");
    while (true) delay(1000);
  }
}

void loop() {
  camera_fb_t *fb = cam.capture(2000);
  if (!fb) return;
  Serial.printf("%ux%u len=%u\n", fb->width, fb->height, (unsigned)fb->len);
  cam.release(fb);
}
```

Serial @ **115200**. Expect lines like `CSI: streaming 800x640 RGB565`.

`cam.capture()` is always **RGB565**. Sensor tables that say RAW10 (OV5647, OV2710, …) mean the **MIPI input**; the ISP converts that to RGB565. Keep `cfg.pixel_format = ESP32P4_PIXFORMAT_RGB565`.

---

## Next steps

1. [API Reference](API-Reference.md) — every method  
2. [HTTP & Preferences](HTTP-and-Preferences.md) — live webcam UI  
3. Example `04_WiFiMjpeg` — full stream + settings  

---

## Compatibility includes

```cpp
#include <ESP32CSI_Vision.h>   // preferred
#include <ESP32P4_Vision.h>    // alias
#include <ESP32P4_Cam.h>       // alias
```
