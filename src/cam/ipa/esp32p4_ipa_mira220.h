/* Generated from Espressif mira220_default.json (ESPRESSIF MIT). */
#pragma once
#include <stdint.h>

static const uint8_t kMira220HasLsc = 0;
static const uint16_t kMira220LscW = 0;
static const uint16_t kMira220LscH = 0;
static const uint16_t kMira220LscN = 0;
static const uint8_t kMira220LscSets = 0;
static const uint16_t kMira220LscCt[] = { 0 };
static const uint16_t *const kMira220LscCh[1][4] = {{ 0, 0, 0, 0 }};

static const float kMira220Ccm[][10] = {
  { 0.0f, 1.650000f, 0.000000f, 0.000000f, 0.000000f, 1.650000f, 0.000000f, 0.000000f, 0.000000f, 1.650000f },
};
static const uint8_t kMira220CcmN = 1;
static const float kMira220LowLumaThr = 28.0f;
static const float kMira220LowLumaM[9] = { 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f };

static const float kMira220AwbRef[][4] = {
  { 0.0f, 0.0f, 0.0f, 0.0f },
};
static const uint8_t kMira220AwbRefN = 0;
static const float kMira220AwbZone[][7] = {
  { 0 },
};
static const uint8_t kMira220AwbZoneN = 0;
static const float kMira220RgMin = 0.3200f;
static const float kMira220RgMax = 0.9700f;
static const float kMira220BgMin = 0.2200f;
static const float kMira220BgMax = 0.8000f;
static const uint8_t kMira220GMin = 16;
static const uint8_t kMira220GMax = 220;
static const float kMira220NewW = 0.300f;
static const float kMira220PrevW = 0.700f;
static const float kMira220RScale = 1.000f;
static const float kMira220BScale = 1.000f;
static const uint32_t kMira220MinCounted = 80;
static const uint8_t kMira220AeTarget = 82;
static const uint8_t kMira220AeLow = 73;
static const uint8_t kMira220AeHigh = 90;
static const uint8_t kMira220AeHiThr = 237;
static const uint8_t kMira220AeHiReg = 4;
static const uint8_t kMira220AeLoThr = 13;
static const uint8_t kMira220AeLoReg = 5;
static const uint8_t kMira220AeWt[25] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const uint8_t kMira220AcHz = 0;
static const float kMira220IncR = 0.3200f;
static const float kMira220DecR = 0.4200f;
static const float kMira220EnvK = 250000.0f;
static const float kMira220EnvSp[] = { -0.005463f, -0.010018f, 0.000000f, 0.033241f, 0.085583f, 0.136704f, 0.160734f, 0.148777f, 0.148777f, 0.160734f, 0.136704f, 0.085583f, 0.033241f, 0.000000f, -0.010018f, -0.005463f };
static const uint8_t kMira220EnvSpN = 16;
static const float kMira220Pwl[][2] = {
  { 0.0f, 0.0f },
};
static const uint8_t kMira220PwlN = 0;
static const float kMira220Blc[][5] = {
  { 1.000f, 13.0f, 13.0f, 13.0f, 13.0f },
};
static const uint8_t kMira220BlcN = 1;
static const uint8_t kMira220BlcStretch = 0;
static const float kMira220Gamma = 1.0000f;
static const uint8_t kMira220ShH = 8;
static const uint8_t kMira220ShL = 3;
static const float kMira220ShHc = 1.7000f;
static const float kMira220ShMc = 1.5000f;
static const uint8_t kMira220ShMat[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const uint8_t kMira220BfLevel = 3;
static const uint8_t kMira220BfMat[9] = { 2, 4, 2, 4, 5, 4, 2, 4, 2 };
static const uint8_t kMira220Sat = 0;
static const uint8_t kMira220Contrast = 132;
