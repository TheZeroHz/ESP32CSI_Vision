/* Generated from Espressif gc2607_default.json (ESPRESSIF MIT). */
#pragma once
#include <stdint.h>

static const uint8_t kGc2607HasLsc = 0;
static const uint16_t kGc2607LscW = 0;
static const uint16_t kGc2607LscH = 0;
static const uint16_t kGc2607LscN = 0;
static const uint8_t kGc2607LscSets = 0;
static const uint16_t kGc2607LscCt[] = { 0 };
static const uint16_t *const kGc2607LscCh[1][4] = {{ 0, 0, 0, 0 }};

static const float kGc2607Ccm[][10] = {
  { 0.0f, 1.480000f, -0.125000f, -0.355000f, -0.125500f, 1.351700f, -0.226200f, -0.055800f, -0.347800f, 1.403700f },
};
static const uint8_t kGc2607CcmN = 1;
static const float kGc2607LowLumaThr = 26.0f;
static const float kGc2607LowLumaM[9] = { 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f };

static const float kGc2607AwbRef[][4] = {
  { 0.0f, 0.0f, 0.0f, 0.0f },
};
static const uint8_t kGc2607AwbRefN = 0;
static const float kGc2607AwbZone[][7] = {
  { 0 },
};
static const uint8_t kGc2607AwbZoneN = 0;
static const float kGc2607RgMin = 0.5730f;
static const float kGc2607RgMax = 0.9096f;
static const float kGc2607BgMin = 0.5368f;
static const float kGc2607BgMax = 0.9634f;
static const uint8_t kGc2607GMin = 91;
static const uint8_t kGc2607GMax = 190;
static const float kGc2607NewW = 0.300f;
static const float kGc2607PrevW = 0.700f;
static const float kGc2607RScale = 1.000f;
static const float kGc2607BScale = 1.000f;
static const uint32_t kGc2607MinCounted = 1000;
static const uint8_t kGc2607AeTarget = 62;
static const uint8_t kGc2607AeLow = 54;
static const uint8_t kGc2607AeHigh = 68;
static const uint8_t kGc2607AeHiThr = 231;
static const uint8_t kGc2607AeHiReg = 3;
static const uint8_t kGc2607AeLoThr = 14;
static const uint8_t kGc2607AeLoReg = 5;
static const uint8_t kGc2607AeWt[25] = { 1, 1, 2, 1, 1, 1, 2, 3, 2, 1, 1, 3, 5, 3, 1, 1, 2, 3, 2, 1, 1, 1, 2, 1, 1 };
static const uint8_t kGc2607AcHz = 0;
static const float kGc2607IncR = 0.3200f;
static const float kGc2607DecR = 0.4200f;
static const float kGc2607EnvK = 0.0f;
static const float kGc2607EnvSp[] = { 0 };
static const uint8_t kGc2607EnvSpN = 0;
static const float kGc2607Pwl[][2] = {
  { 0.0f, 0.0f },
};
static const uint8_t kGc2607PwlN = 0;
static const float kGc2607Blc[][5] = {
  { 1.000f, 16.0f, 16.0f, 16.0f, 16.0f },
};
static const uint8_t kGc2607BlcN = 1;
static const uint8_t kGc2607BlcStretch = 0;
static const float kGc2607Gamma = 0.6120f;
static const uint8_t kGc2607ShH = 25;
static const uint8_t kGc2607ShL = 5;
static const float kGc2607ShHc = 1.9250f;
static const float kGc2607ShMc = 1.8250f;
static const uint8_t kGc2607ShMat[9] = { 1, 2, 1, 2, 2, 2, 1, 2, 1 };
static const uint8_t kGc2607BfLevel = 3;
static const uint8_t kGc2607BfMat[9] = { 2, 4, 2, 4, 5, 4, 2, 4, 2 };
static const uint8_t kGc2607Sat = 128;
static const uint8_t kGc2607Contrast = 132;
