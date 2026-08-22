/* Generated from Espressif ov9281_default_p4_eco5.json (ESPRESSIF MIT). */
#pragma once
#include <stdint.h>

static const uint8_t kOv9281HasLsc = 0;
static const uint16_t kOv9281LscW = 0;
static const uint16_t kOv9281LscH = 0;
static const uint16_t kOv9281LscN = 0;
static const uint8_t kOv9281LscSets = 0;
static const uint16_t kOv9281LscCt[] = { 0 };
static const uint16_t *const kOv9281LscCh[1][4] = {{ 0, 0, 0, 0 }};

static const float kOv9281Ccm[][10] = {
  { 0.0f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f },
};
static const uint8_t kOv9281CcmN = 1;
static const float kOv9281LowLumaThr = 28.0f;
static const float kOv9281LowLumaM[9] = { 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f };

static const float kOv9281AwbRef[][4] = {
  { 0.0f, 0.0f, 0.0f, 0.0f },
};
static const uint8_t kOv9281AwbRefN = 0;
static const float kOv9281AwbZone[][7] = {
  { 0 },
};
static const uint8_t kOv9281AwbZoneN = 0;
static const float kOv9281RgMin = 0.3200f;
static const float kOv9281RgMax = 0.9700f;
static const float kOv9281BgMin = 0.2200f;
static const float kOv9281BgMax = 0.8000f;
static const uint8_t kOv9281GMin = 16;
static const uint8_t kOv9281GMax = 220;
static const float kOv9281NewW = 0.300f;
static const float kOv9281PrevW = 0.700f;
static const float kOv9281RScale = 1.000f;
static const float kOv9281BScale = 1.000f;
static const uint32_t kOv9281MinCounted = 80;
static const uint8_t kOv9281AeTarget = 19;
static const uint8_t kOv9281AeLow = 17;
static const uint8_t kOv9281AeHigh = 24;
static const uint8_t kOv9281AeHiThr = 212;
static const uint8_t kOv9281AeHiReg = 5;
static const uint8_t kOv9281AeLoThr = 6;
static const uint8_t kOv9281AeLoReg = 9;
static const uint8_t kOv9281AeWt[25] = { 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 2, 3, 2, 1, 1, 2, 2, 2, 1, 1, 1, 2, 1, 1 };
static const uint8_t kOv9281AcHz = 0;
static const float kOv9281IncR = 0.3200f;
static const float kOv9281DecR = 0.4200f;
static const float kOv9281EnvK = 250000.0f;
static const float kOv9281EnvSp[] = { -0.005463f, -0.010018f, 0.000000f, 0.033241f, 0.085583f, 0.136704f, 0.160734f, 0.148777f, 0.148777f, 0.160734f, 0.136704f, 0.085583f, 0.033241f, 0.000000f, -0.010018f, -0.005463f };
static const uint8_t kOv9281EnvSpN = 16;
static const float kOv9281Pwl[][2] = {
  { 0.0f, 0.0f },
};
static const uint8_t kOv9281PwlN = 0;
static const float kOv9281Blc[][5] = {
  { 1.000f, 16.0f, 16.0f, 16.0f, 16.0f },
  { 10.000f, 15.0f, 15.0f, 15.0f, 15.0f },
};
static const uint8_t kOv9281BlcN = 2;
static const uint8_t kOv9281BlcStretch = 0;
static const float kOv9281Gamma = 0.6450f;
static const uint8_t kOv9281ShH = 22;
static const uint8_t kOv9281ShL = 5;
static const float kOv9281ShHc = 1.7250f;
static const float kOv9281ShMc = 1.6250f;
static const uint8_t kOv9281ShMat[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const uint8_t kOv9281BfLevel = 3;
static const uint8_t kOv9281BfMat[9] = { 2, 4, 2, 4, 5, 4, 2, 4, 2 };
static const uint8_t kOv9281Sat = 0;
static const uint8_t kOv9281Contrast = 132;
