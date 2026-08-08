# Vendored espressif/esp_h264 (HW encoder subset)

Arduino-ESP32 does not ship `esp_h264`. This tree is packaged for **ESP32-P4** Arduino builds of ESP32CSI_Vision.

- Public headers: `../` (`src/*.h` on the library include path)
- Sources: this folder (Arduino compiles recursively; guarded with `CONFIG_IDF_TARGET_ESP32P4`)
- ESP-IDF: uses Component Manager `espressif/esp_h264` via root `idf_component.yml` — this folder is **not** listed in `CMakeLists.txt` `SRC_DIRS`

Upstream: https://github.com/espressif/esp-h264-component (Apache-2.0)
