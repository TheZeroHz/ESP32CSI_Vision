/* Generated from Espressif ov5647_default.json (ESPRESSIF MIT). */
#pragma once
#include <stdint.h>

static const uint8_t kOv5647HasLsc = 0;
static const uint16_t kOv5647LscW = 0;
static const uint16_t kOv5647LscH = 0;
static const uint16_t kOv5647LscN = 0;
static const uint8_t kOv5647LscSets = 0;
static const uint16_t kOv5647LscCt[] = { 0 };
static const uint16_t *const kOv5647LscCh[1][4] = {{ 0, 0, 0, 0 }};

static const float kOv5647Ccm[][10] = {
  { 0.0f, 2.000000f, -0.545900f, -0.454100f, -0.475100f, 1.769600f, -0.294500f, -0.200200f, -0.799800f, 2.000000f },
};
static const uint8_t kOv5647CcmN = 1;
static const float kOv5647LowLumaThr = 28.0f;
static const float kOv5647LowLumaM[9] = { 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f };

static const float kOv5647AwbRef[][4] = {
  { 0.0f, 0.0f, 0.0f, 0.0f },
};
static const uint8_t kOv5647AwbRefN = 0;
static const float kOv5647AwbZone[][7] = {
  { 0 },
};
static const uint8_t kOv5647AwbZoneN = 0;
static const float kOv5647RgMin = 0.3200f;
static const float kOv5647RgMax = 0.9700f;
static const float kOv5647BgMin = 0.2200f;
static const float kOv5647BgMax = 0.8000f;
static const uint8_t kOv5647GMin = 16;
static const uint8_t kOv5647GMax = 220;
static const float kOv5647NewW = 0.300f;
static const float kOv5647PrevW = 0.700f;
static const float kOv5647RScale = 1.000f;
static const float kOv5647BScale = 1.000f;
static const uint32_t kOv5647MinCounted = 80;
static const uint8_t kOv5647AeTarget = 80;
static const uint8_t kOv5647AeLow = 70;
static const uint8_t kOv5647AeHigh = 90;
static const uint8_t kOv5647AeHiThr = 239;
static const uint8_t kOv5647AeHiReg = 3;
static const uint8_t kOv5647AeLoThr = 13;
static const uint8_t kOv5647AeLoReg = 5;
static const uint8_t kOv5647AeWt[25] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const uint8_t kOv5647AcHz = 0;
static const float kOv5647IncR = 0.3200f;
static const float kOv5647DecR = 0.4200f;
static const float kOv5647EnvK = 0.0f;
static const float kOv5647EnvSp[] = { 0 };
static const uint8_t kOv5647EnvSpN = 0;
static const float kOv5647Pwl[][2] = {
  { 0.0f, 0.0f },
};
static const uint8_t kOv5647PwlN = 0;
static const float kOv5647Blc[][5] = {
  { 1.000f, 16.0f, 16.0f, 16.0f, 16.0f },
};
static const uint8_t kOv5647BlcN = 1;
static const uint8_t kOv5647BlcStretch = 0;
static const float kOv5647Gamma = 0.7200f;
static const uint8_t kOv5647ShH = 56;
static const uint8_t kOv5647ShL = 10;
static const float kOv5647ShHc = 0.4250f;
static const float kOv5647ShMc = 0.6250f;
static const uint8_t kOv5647ShMat[9] = { 1, 2, 1, 2, 2, 2, 1, 2, 1 };
static const uint8_t kOv5647BfLevel = 5;
static const uint8_t kOv5647BfMat[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 };
static const uint8_t kOv5647Sat = 128;
static const uint8_t kOv5647Contrast = 134;
