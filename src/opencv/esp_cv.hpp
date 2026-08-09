#pragma once

/**
 * ESP32CSI lean OpenCV-compatible API (esp_cv).
 * Phase 1: Core types + Mat (PSRAM / external / ROI / stride).
 * Existing ESP32P4_Cv / Ppa continue to power imgproc; Mat wrappers land in later phases.
 */

#include "opencv/esp_cv_mat.h"
#include "opencv/esp_cv_types.h"

// Optional OpenCV-like aliases in global cv:: for familiarity (does not pull real OpenCV).
#ifndef ESP_CV_NO_CV_ALIAS
namespace cv {
using esp_cv::Mat;
using esp_cv::Point;
using esp_cv::Point2f;
using esp_cv::Range;
using esp_cv::Rect;
using esp_cv::Scalar;
using esp_cv::Size;
using esp_cv::CV_8UC1;
using esp_cv::CV_8UC2;
using esp_cv::CV_8UC3;
using esp_cv::CV_8UC4;
using esp_cv::CV_16UC1;
using esp_cv::CV_16SC1;
using esp_cv::CV_32FC1;
using esp_cv::CV_32FC2;
using esp_cv::CV_32FC3;
using esp_cv::wrapGray;
using esp_cv::wrapRgb565;
using esp_cv::wrapRgb888;
}  // namespace cv
#endif
