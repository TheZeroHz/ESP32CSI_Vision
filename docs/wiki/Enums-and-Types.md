# Enums & Types

## Boards — `esp32p4_board_t`

| Enum | Value | Board |
| --- | :---: | --- |
| `ESP32P4_BOARD_GUITION_M3` | 0 | Guition JC-ESP32P4-M3 (default) |
| `ESP32P4_BOARD_WAVESHARE_NANO` | 1 | Waveshare ESP32-P4 Nano |
| `ESP32P4_BOARD_FUNCTION_EV` | 2 | Espressif Function EV |
| `ESP32P4_BOARD_CUSTOM` | 3 | Use full `esp32p4_cam_config_t` |

```cpp
cam.begin(ESP32P4_BOARD_GUITION_M3);
```

---

## Sensors — `esp32p4_cam_sensor_t`

| Enum | Value | Notes |
| --- | :---: | --- |
| `ESP32P4_SENSOR_AUTO` | 0 | Probe |
| `ESP32P4_SENSOR_OV5647` | 1 | Common CSI module (`0x36`) |
| `ESP32P4_SENSOR_IMX708` | 2 | Pi Camera Module 3 class |

```cpp
cfg.sensor = ESP32P4_SENSOR_OV5647;
```

---

## Native framesize — `esp32p4_cam_framesize_t`

CSI / ISP capture size (boot config).

| Enum | Value | Typical size |
| --- | :---: | --- |
| `ESP32P4_FRAMESIZE_800X640` | 0 | 800×640 (default) |
| `ESP32P4_FRAMESIZE_HD` | 1 | HD-class mode |
| `ESP32P4_FRAMESIZE_2304X1296` | 2 | High-res mode (memory heavy) |

```cpp
cfg.frame_size = ESP32P4_FRAMESIZE_800X640;
```

---

## Pixel format — `esp32p4_cam_pixformat_t`

| Enum | Value | Bytes/pixel | Notes |
| --- | :---: | :---: | --- |
| `ESP32P4_PIXFORMAT_RGB565` | 0 | 2 | Default for JPEG / MJPEG / DSP |
| `ESP32P4_PIXFORMAT_RAW10` | 1 | — | Raw path (advanced) |

---

## Stream framesize — `esp32p4_stream_framesize_t`

MJPEG **output** size (PPA scale from native). Runtime only.

| Enum | Value | Size |
| --- | :---: | --- |
| `ESP32P4_STREAM_SVGA` | 0 | 800×640 |
| `ESP32P4_STREAM_VGA` | 1 | 640×480 |
| `ESP32P4_STREAM_HVGA` | 2 | 480×320 |
| `ESP32P4_STREAM_QVGA` | 3 | 320×240 |
| `ESP32P4_STREAM_QQVGA` | 4 | 160×120 |

```cpp
stream.setFramesize(ESP32P4_STREAM_QVGA);
// HTTP: /control?var=framesize&val=3
```

---

## Face models — `ESP32P4_FaceDetect::Model`

| Enum | Value | Use |
| --- | :---: | --- |
| `MSRMNP_S8_V1` | 0 | Default |
| `ESPDET_PICO_224` | 1 | Smaller input |
| `ESPDET_PICO_416` | 2 | Larger input |

---

## Structs (quick)

### `camera_fb_t`

```cpp
struct camera_fb_t {
  uint8_t *buf;
  size_t len;
  uint16_t width;
  uint16_t height;
  esp32p4_cam_pixformat_t format;
  uint32_t timestamp_us;
};
```

### `esp32p4_cam_config_t`

See [API Reference — Camera config](API-Reference.md#camera-config--helpers).

### `esp32p4_rect_t`

```cpp
struct esp32p4_rect_t { int x, y, w, h; };
```

### `esp32p4_motion_t`

```cpp
struct esp32p4_motion_t {
  bool moving;
  uint32_t changed;
  uint32_t total;
  esp32p4_rect_t roi;
};
```

### `esp32p4_who_fb_t`

```cpp
struct esp32p4_who_fb_t {
  uint8_t *buf;
  size_t len;
  uint16_t width;
  uint16_t height;
  uint32_t timestamp_us;
  void *user;
};
```

### `esp32p4_face_t`

```cpp
struct esp32p4_face_t {
  float score;
  int x, y, w, h;
  int landmarks[5][2];
  bool has_landmarks;
};
```

---

## Type aliases

```cpp
using ESP32P4_CSI_Camera = ESP32P4_Camera;
```

← [API Reference](API-Reference.md) · [Home](Home.md) →
