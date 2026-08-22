# 28_DualCam

Two `ESP32P4_Camera` objects at once. Same `capture()` path.

ESP32-P4 has **one MIPI CSI host**. Dual means CSI plus DVP (default), SPI, or USB-host UVC — not two CSI modules.

Fill `DVP_*` pins (same as `24_DvpCam`). `#define APP_SECOND_BUS ESP32P4_CAM_BUS_SPI` or `ESP32P4_CAM_BUS_UVC_HOST` for the other second bus. If cam1 fails, CSI still runs.
