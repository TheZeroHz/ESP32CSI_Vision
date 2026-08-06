#pragma once

/**
 * ESP32CSI_Vision — ESP32 MIPI CSI computer-vision library
 * Author: Rakib Hasan (@thezerohz)
 *
 * Processor-agnostic name for CSI vision on Espressif chips
 * (ESP32-P4 today; other CSI-capable SoCs later).
 *
 * Capture, PSRAM frames, HW JPEG/H.264, PPA, DSP, MJPEG UI,
 * WHO-style pipelines, microSD (SDMMC), ESP-DL face detect (IDF).
 */

#include "cam/ESP32P4_Camera.h"
#include "dsp/ESP32P4_Dsp.h"
#include "h264/ESP32P4_H264.h"
#include "h264/ESP32P4_H264Mp4.h"
#include "img/ESP32P4_Img.h"
#include "jpeg/ESP32P4_Jpeg.h"
#include "mem/ESP32P4_Psram.h"
#include "ppa/ESP32P4_Ppa.h"
#include "sd/ESP32P4_Sd.h"
#include "stream/ESP32P4_Mjpeg.h"
#include "who/ESP32P4_Who.h"
