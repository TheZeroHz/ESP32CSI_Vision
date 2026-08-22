/* Generated from Espressif sc2331_default.json (ESPRESSIF MIT). */
#pragma once
#include <stdint.h>

static const uint8_t kSc2331HasLsc = 0;
static const uint16_t kSc2331LscW = 0;
static const uint16_t kSc2331LscH = 0;
static const uint16_t kSc2331LscN = 0;
static const uint8_t kSc2331LscSets = 0;
static const uint16_t kSc2331LscCt[] = { 0 };
static const uint16_t *const kSc2331LscCh[1][4] = {{ 0, 0, 0, 0 }};

static const float kSc2331Ccm[][10] = {
  { 1200.0f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f },
  { 2800.0f, 1.278400f, -0.278800f, 0.000400f, -1.054500f, 2.715900f, -0.661400f, -0.289300f, -0.883700f, 2.173000f },
  { 3000.0f, 1.616400f, -0.617100f, 0.000700f, -0.686500f, 1.921900f, -0.235300f, -0.156900f, -0.812300f, 1.969300f },
  { 3500.0f, 1.577900f, -0.319600f, -0.258400f, -0.491800f, 1.669600f, -0.177900f, -0.103600f, -0.625200f, 1.728800f },
  { 4000.0f, 1.556800f, -0.300100f, -0.256700f, -0.452000f, 1.689600f, -0.237500f, -0.101100f, -0.516200f, 1.617400f },
  { 5000.0f, 1.608100f, -0.474300f, -0.133800f, -0.388900f, 1.695200f, -0.306400f, -0.072300f, -0.665600f, 1.737900f },
  { 6500.0f, 1.326300f, -0.031500f, -0.294800f, -0.346300f, 1.639600f, -0.293300f, -0.088000f, -0.481600f, 1.569700f },
  { 12000.0f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f },
};
static const uint8_t kSc2331CcmN = 8;
static const float kSc2331LowLumaThr = 100.0f;
static const float kSc2331LowLumaM[9] = { 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f };

static const float kSc2331AwbRef[][4] = {
  { 2800.0f, 0.847400f, 0.313800f, 0.0200f },
  { 3000.0f, 0.783000f, 0.309100f, 0.0300f },
  { 3500.0f, 0.676100f, 0.352500f, 0.0400f },
  { 4000.0f, 0.617100f, 0.397400f, 0.0400f },
  { 5000.0f, 0.550200f, 0.478100f, 0.0400f },
  { 6500.0f, 0.453100f, 0.552500f, 0.0400f },
};
static const uint8_t kSc2331AwbRefN = 6;
static const float kSc2331AwbZone[][7] = {
  { 0.0f, 0.3800f, 0.5200f, 0.5000f, 0.6200f, 1.0f, 0.0f },
  { 1.0f, 0.4700f, 0.6200f, 0.4200f, 0.5400f, 1.0f, 0.0f },
  { 2.0f, 0.5800f, 0.7200f, 0.3200f, 0.4600f, 1.0f, 0.0f },
  { 3.0f, 0.7000f, 0.8600f, 0.2800f, 0.3600f, 1.0f, 0.0f },
  { 4.0f, 0.8200f, 1.0500f, 0.2200f, 0.3500f, 0.0f, 0.0f },
};
static const uint8_t kSc2331AwbZoneN = 5;
static const float kSc2331RgMin = 0.3200f;
static const float kSc2331RgMax = 0.9700f;
static const float kSc2331BgMin = 0.2200f;
static const float kSc2331BgMax = 0.8000f;
static const uint8_t kSc2331GMin = 18;
static const uint8_t kSc2331GMax = 208;
static const float kSc2331NewW = 0.300f;
static const float kSc2331PrevW = 0.700f;
static const float kSc2331RScale = 1.000f;
static const float kSc2331BScale = 1.000f;
static const uint32_t kSc2331MinCounted = 10;
static const uint8_t kSc2331AeTarget = 45;
static const uint8_t kSc2331AeLow = 40;
static const uint8_t kSc2331AeHigh = 50;
static const uint8_t kSc2331AeHiThr = 230;
static const uint8_t kSc2331AeHiReg = 3;
static const uint8_t kSc2331AeLoThr = 13;
static const uint8_t kSc2331AeLoReg = 5;
static const uint8_t kSc2331AeWt[25] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const uint8_t kSc2331AcHz = 0;
static const float kSc2331IncR = 0.3200f;
static const float kSc2331DecR = 0.4200f;
static const float kSc2331EnvK = 495432.0f;
static const float kSc2331EnvSp[] = { -0.005463f, -0.010018f, 0.000000f, 0.033241f, 0.085583f, 0.136704f, 0.160734f, 0.148777f, 0.148777f, 0.160734f, 0.136704f, 0.085583f, 0.033241f, 0.000000f, -0.010018f, -0.005463f };
static const uint8_t kSc2331EnvSpN = 16;
static const float kSc2331Pwl[][2] = {
  { 10.00f, -30.0f },
  { 90.00f, -10.0f },
  { 100.00f, -10.0f },
  { 432.00f, -10.0f },
  { 700.00f, -10.0f },
  { 900.00f, -10.0f },
  { 5000.00f, 10.0f },
  { 10000.00f, 5.0f },
  { 12000.00f, 0.0f },
};
static const uint8_t kSc2331PwlN = 9;
static const float kSc2331Blc[][5] = {
  { 1.000f, 16.0f, 16.0f, 16.0f, 16.0f },
};
static const uint8_t kSc2331BlcN = 1;
static const uint8_t kSc2331BlcStretch = 0;
static const float kSc2331Gamma = 0.5850f;
static const uint8_t kSc2331ShH = 16;
static const uint8_t kSc2331ShL = 5;
static const float kSc2331ShHc = 1.6250f;
static const float kSc2331ShMc = 1.5250f;
static const uint8_t kSc2331ShMat[9] = { 1, 2, 1, 2, 3, 2, 1, 2, 1 };
static const uint8_t kSc2331BfLevel = 10;
static const uint8_t kSc2331BfMat[9] = { 1, 3, 1, 3, 4, 3, 1, 3, 1 };
static const uint8_t kSc2331Sat = 130;
static const uint8_t kSc2331Contrast = 132;
