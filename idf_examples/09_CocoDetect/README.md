# 09_CocoDetect — ESP-DL COCO object detection (ESP-IDF + Arduino)

Serial-only demo: CSI RGB565 → YOLO11n COCO-80 labels (person, bus, car, …).

## Models (SD card)

Copy from library `models/espdl/p4/` to the SD root:

```
/models/p4/coco_detect_yolo11n_320_s8_v1.espdl   (default in this example)
/models/p4/coco_detect_yolo11n_s8_v1.espdl       (optional 640)
/models/p4/pedestrian_detect_pico_s8_v1.espdl    (optional)
```

sdkconfig defaults to `CONFIG_COCO_DETECT_MODEL_IN_SDCARD`.

## Build

```bat
cd idf_examples\09_CocoDetect
idf.py -DSDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32p4 set-target esp32p4
idf.py -p COM8 build flash monitor
```

## Switch model

```cpp
det.begin(ESP32P4_ObjectDetect::COCO_YOLO11N);      // 640
det.begin(ESP32P4_ObjectDetect::COCO_YOLO11N_320);  // 320 (default)
det.begin(ESP32P4_ObjectDetect::PEDESTRIAN_PICO);   // pedestrian
```

## Web UI

See Arduino example `examples/32_EthCocoWeb`.
