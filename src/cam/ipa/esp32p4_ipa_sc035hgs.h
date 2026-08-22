/* Generated from Espressif sc035hgs_rgb_default.json (ESPRESSIF MIT). */
#pragma once
#include <stdint.h>

static const uint8_t kSc035hgsHasLsc = 0;
static const uint16_t kSc035hgsLscW = 0;
static const uint16_t kSc035hgsLscH = 0;
static const uint16_t kSc035hgsLscN = 0;
static const uint8_t kSc035hgsLscSets = 0;
static const uint16_t kSc035hgsLscCt[] = { 0 };
static const uint16_t *const kSc035hgsLscCh[1][4] = {{ 0, 0, 0, 0 }};

static const float kSc035hgsCcm[][10] = {
  { 0.0f, 1.480000f, -0.125000f, -0.355000f, -0.125500f, 1.351700f, -0.226200f, -0.055800f, -0.347800f, 1.403700f },
};
static const uint8_t kSc035hgsCcmN = 1;
static const float kSc035hgsLowLumaThr = 26.0f;
static const float kSc035hgsLowLumaM[9] = { 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f };

static const float kSc035hgsAwbRef[][4] = {
  { 0.0f, 0.0f, 0.0f, 0.0f },
};
static const uint8_t kSc035hgsAwbRefN = 0;
static const float kSc035hgsAwbZone[][7] = {
  { 0 },
};
static const uint8_t kSc035hgsAwbZoneN = 0;
static const float kSc035hgsRgMin = 0.5730f;
static const float kSc035hgsRgMax = 0.9096f;
static const float kSc035hgsBgMin = 0.5368f;
static const float kSc035hgsBgMax = 0.9634f;
static const uint8_t kSc035hgsGMin = 91;
static const uint8_t kSc035hgsGMax = 190;
static const float kSc035hgsNewW = 0.300f;
static const float kSc035hgsPrevW = 0.700f;
static const float kSc035hgsRScale = 1.000f;
static const float kSc035hgsBScale = 1.000f;
static const uint32_t kSc035hgsMinCounted = 1000;
static const uint8_t kSc035hgsAeTarget = 62;
static const uint8_t kSc035hgsAeLow = 54;
static const uint8_t kSc035hgsAeHigh = 68;
static const uint8_t kSc035hgsAeHiThr = 231;
static const uint8_t kSc035hgsAeHiReg = 3;
static const uint8_t kSc035hgsAeLoThr = 14;
static const uint8_t kSc035hgsAeLoReg = 5;
static const uint8_t kSc035hgsAeWt[25] = { 1, 1, 2, 1, 1, 1, 2, 3, 2, 1, 1, 3, 5, 3, 1, 1, 2, 3, 2, 1, 1, 1, 2, 1, 1 };
static const uint8_t kSc035hgsAcHz = 0;
static const float kSc035hgsIncR = 0.3200f;
static const float kSc035hgsDecR = 0.4200f;
static const float kSc035hgsEnvK = 0.0f;
static const float kSc035hgsEnvSp[] = { 0 };
static const uint8_t kSc035hgsEnvSpN = 0;
static const float kSc035hgsPwl[][2] = {
  { 0.0f, 0.0f },
};
static const uint8_t kSc035hgsPwlN = 0;
static const float kSc035hgsBlc[][5] = {
  { 1.000f, 16.0f, 16.0f, 16.0f, 16.0f },
};
static const uint8_t kSc035hgsBlcN = 1;
static const uint8_t kSc035hgsBlcStretch = 0;
static const float kSc035hgsGamma = 0.6120f;
static const uint8_t kSc035hgsShH = 25;
static const uint8_t kSc035hgsShL = 5;
static const float kSc035hgsShHc = 1.9250f;
static const float kSc035hgsShMc = 1.8250f;
static const uint8_t kSc035hgsShMat[9] = { 1, 2, 1, 2, 2, 2, 1, 2, 1 };
static const uint8_t kSc035hgsBfLevel = 3;
static const uint8_t kSc035hgsBfMat[9] = { 2, 4, 2, 4, 5, 4, 2, 4, 2 };
static const uint8_t kSc035hgsSat = 128;
static const uint8_t kSc035hgsContrast = 132;
