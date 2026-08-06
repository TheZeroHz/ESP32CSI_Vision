# ESP32CSI_Vision as an ESP-IDF component (Arduino IDE ignores this file).
# Arduino auto-compiles only sources under src/; ESP-DL face detect lives in idf_src/.

idf_component_register(
    SRC_DIRS
        "src"
        "src/cam"
        "src/cam/sensors"
        "src/mem"
        "src/jpeg"
        "src/img"
        "src/ppa"
        "src/dsp"
        "src/h264"
        "src/sd"
        "src/stream"
        "src/who"
        "idf_src"
    INCLUDE_DIRS
        "src"
        "idf_src"
    REQUIRES
        arduino-esp32
        human_face_detect
        esp_h264
    PRIV_REQUIRES
        esp_driver_cam
        esp_driver_isp
        esp_driver_jpeg
        esp_driver_ppa
        esp_driver_ledc
        esp_mm
        esp_psram
        esp_timer
        esp_hw_support
        freertos
        heap
)
