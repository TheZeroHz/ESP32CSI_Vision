/* Generated from Espressif ov2710_default.json (ESPRESSIF MIT). */
#pragma once
#include <stdint.h>

static const uint8_t kOv2710HasLsc = 0;
static const uint16_t kOv2710LscW = 0;
static const uint16_t kOv2710LscH = 0;
static const uint16_t kOv2710LscN = 0;
static const uint8_t kOv2710LscSets = 0;
static const uint16_t kOv2710LscCt[] = { 0 };
static const uint16_t *const kOv2710LscCh[1][4] = {{ 0, 0, 0, 0 }};

static const float kOv2710Ccm[][10] = {
  { 1200.0f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f },
  { 2314.0f, 2.243600f, -0.334700f, -0.908900f, -0.412900f, 1.314400f, 0.098500f, -0.510300f, -2.489500f, 3.999900f },
  { 2882.0f, 2.311600f, -0.409100f, -0.902500f, -0.380700f, 1.588200f, -0.207500f, -0.317500f, -1.856200f, 3.173600f },
  { 3372.0f, 2.040600f, -0.713700f, -0.326900f, -0.371700f, 1.426200f, -0.054400f, -0.458400f, -1.383100f, 2.841500f },
  { 3880.0f, 2.066400f, -0.752400f, -0.314000f, -0.339200f, 1.436200f, -0.097000f, -0.387300f, -1.160400f, 2.547800f },
  { 4310.0f, 2.060600f, -0.760800f, -0.299800f, -0.315000f, 1.416300f, -0.101200f, -0.366400f, -0.983600f, 2.350000f },
  { 4679.0f, 2.111200f, -0.850400f, -0.260800f, -0.297400f, 1.401200f, -0.103800f, -0.364800f, -0.909900f, 2.274700f },
  { 5097.0f, 2.112000f, -0.829900f, -0.282100f, -0.285700f, 1.399200f, -0.113500f, -0.354600f, -0.845200f, 2.199800f },
  { 5470.0f, 2.156300f, -0.827000f, -0.329300f, -0.198200f, 1.356100f, -0.157900f, -0.207800f, -0.981100f, 2.188900f },
  { 5780.0f, 2.070100f, -0.707000f, -0.363100f, -0.231000f, 1.446700f, -0.215700f, -0.279400f, -0.758700f, 2.038000f },
  { 6086.0f, 2.097100f, -0.731000f, -0.366100f, -0.228500f, 1.437400f, -0.209000f, -0.271600f, -0.736100f, 2.007700f },
  { 6670.0f, 2.082200f, -0.565700f, -0.516500f, -0.203600f, 1.469600f, -0.266100f, -0.161300f, -0.858500f, 2.019800f },
  { 7200.0f, 2.021700f, -0.480500f, -0.541200f, -0.200700f, 1.501600f, -0.300900f, -0.158700f, -0.736800f, 1.895500f },
  { 7847.0f, 2.087600f, -0.567100f, -0.520500f, -0.196700f, 1.473700f, -0.277000f, -0.163800f, -0.715400f, 1.879200f },
  { 16000.0f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f },
};
static const uint8_t kOv2710CcmN = 15;
static const float kOv2710LowLumaThr = 28.0f;
static const float kOv2710LowLumaM[9] = { 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 0.000000f, 1.000000f };

static const float kOv2710AwbRef[][4] = {
  { 0.0f, 0.0f, 0.0f, 0.0f },
};
static const uint8_t kOv2710AwbRefN = 0;
static const float kOv2710AwbZone[][7] = {
  { 0 },
};
static const uint8_t kOv2710AwbZoneN = 0;
static const float kOv2710RgMin = 0.3200f;
static const float kOv2710RgMax = 0.9700f;
static const float kOv2710BgMin = 0.2200f;
static const float kOv2710BgMax = 0.8000f;
static const uint8_t kOv2710GMin = 16;
static const uint8_t kOv2710GMax = 220;
static const float kOv2710NewW = 0.300f;
static const float kOv2710PrevW = 0.700f;
static const float kOv2710RScale = 1.000f;
static const float kOv2710BScale = 1.000f;
static const uint32_t kOv2710MinCounted = 80;
static const uint8_t kOv2710AeTarget = 80;
static const uint8_t kOv2710AeLow = 70;
static const uint8_t kOv2710AeHigh = 90;
static const uint8_t kOv2710AeHiThr = 239;
static const uint8_t kOv2710AeHiReg = 3;
static const uint8_t kOv2710AeLoThr = 13;
static const uint8_t kOv2710AeLoReg = 5;
static const uint8_t kOv2710AeWt[25] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const uint8_t kOv2710AcHz = 0;
static const float kOv2710IncR = 0.3200f;
static const float kOv2710DecR = 0.4200f;
static const float kOv2710EnvK = 250000.0f;
static const float kOv2710EnvSp[] = { -0.005463f, -0.010018f, 0.000000f, 0.033241f, 0.085583f, 0.136704f, 0.160734f, 0.148777f, 0.148777f, 0.160734f, 0.136704f, 0.085583f, 0.033241f, 0.000000f, -0.010018f, -0.005463f };
static const uint8_t kOv2710EnvSpN = 16;
static const float kOv2710Pwl[][2] = {
  { 0.0f, 0.0f },
};
static const uint8_t kOv2710PwlN = 0;
static const float kOv2710Blc[][5] = {
  { 1.000f, 16.0f, 16.0f, 16.0f, 16.0f },
};
static const uint8_t kOv2710BlcN = 1;
static const uint8_t kOv2710BlcStretch = 0;
static const float kOv2710Gamma = 0.5300f;
static const uint8_t kOv2710ShH = 16;
static const uint8_t kOv2710ShL = 3;
static const float kOv2710ShHc = 2.6500f;
static const float kOv2710ShMc = 2.9500f;
static const uint8_t kOv2710ShMat[9] = { 1, 2, 1, 2, 2, 2, 1, 2, 1 };
static const uint8_t kOv2710BfLevel = 3;
static const uint8_t kOv2710BfMat[9] = { 1, 3, 1, 3, 5, 3, 1, 3, 1 };
static const uint8_t kOv2710Sat = 128;
static const uint8_t kOv2710Contrast = 130;
