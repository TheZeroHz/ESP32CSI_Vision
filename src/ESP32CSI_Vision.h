#pragma once

/**
 * ESP32CSI_Vision — ESP32 MIPI CSI computer-vision library
 * Author: Rakib Hasan (@thezerohz)
 *
 * Processor-agnostic name for CSI vision on Espressif chips
 * (ESP32-P4 today; other CSI-capable SoCs later).
 *
 * Capture, PSRAM frames, HW JPEG/H.264, PPA, DSP, OpenCV-like CV,
 * Vision AI helpers (letterbox/NMS), MJPEG UI, WHO-style pipelines,
 * microSD, bundled WebFileManager, vendored ESP-DL face detect/recognize.
 */

#include "audio/ESP32P4_Mic.h"
#include "cam/ESP32P4_Camera.h"
#include "cv/ESP32P4_Cv.h"
#include "cv/ESP32P4_CvDash.h"
#include "cv/ESP32P4_Tracker.h"
#include "dsp/ESP32P4_Dsp.h"
#include "face/ESP32P4_FaceAi.h"
#include "face/ESP32P4_FaceDetect.h"
#include "h264/ESP32P4_H264.h"
#include "h264/ESP32P4_H264Mp4.h"
#include "img/ESP32P4_Img.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "mem/ESP32P4_Psram.h"
#include "opencv/esp_cv.hpp"
#include "ppa/ESP32P4_Ppa.h"
#include "sd/ESP32P4_Sd.h"
#include "storage/ESP32P4_StoragePref.h"
#include "stream/ESP32P4_Mjpeg.h"
#include "vision/ESP32P4_VisionAi.h"
#include "wfm/WebFileManager.h"
#include "who/ESP32P4_Who.h"
#include "qr/ESP32P4_Qr.h"
#include "cam/ESP32P4_SmartAe.h"
