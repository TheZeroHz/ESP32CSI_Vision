#include "cam/sensors/ov5647_sensor.h"

#include <Arduino.h>
#include <Wire.h>

#include "cam/sensors/imx708_sensor.h"

#define OV5647_ADDR 0x36
#define OV5647_REG_END 0xFFFF
#define OV5647_REG_DELAY 0xEEEE
#define OV5647_8BIT_MODE 0x18
#define OV5647_10BIT_MODE 0x1A
#define OV5647_IDI_CLOCK_RATE_800x640_50FPS 100000000ULL

typedef struct {
  uint16_t reg;
  uint8_t val;
} ov5647_reginfo_t;

static uint16_t s_exp_max_lines = 980;

static const ov5647_reginfo_t ov5647_reset[] = {
    {0x0100, 0x00},
    {0x0103, 0x01},
    {OV5647_REG_DELAY, 0x0A},
    {0x4800, 0x01},
    {OV5647_REG_END, 0x00},
};

static const ov5647_reginfo_t ov5647_800x640_raw8[] = {
    {0x3034, OV5647_8BIT_MODE},
    {0x3035, 0x41},
    {0x3036, (uint8_t)((OV5647_IDI_CLOCK_RATE_800x640_50FPS * 8 * 4) / 25000000)},
    {0x303c, 0x11},
    {0x3106, 0xf5},
    {0x3821, 0x03},
    {0x3820, 0x41},
    {0x3827, 0xec},
    {0x370c, 0x0f},
    {0x3612, 0x59},
    {0x3618, 0x00},
    {0x5000, 0xff},
    {0x583e, 0xf0},
    {0x583f, 0x20},
    {0x5002, 0x41},
    {0x5003, 0x08},
    {0x5a00, 0x08},
    {0x3000, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3016, 0x08},
    {0x3017, 0xe0},
    {0x3018, 0x44},
    {0x301c, 0xf8},
    {0x301d, 0xf0},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3c01, 0x80},
    {0x3c00, 0x40},
    {0x3b07, 0x0c},
    {0x380c, (1896 >> 8) & 0x1F},
    {0x380d, 1896 & 0xFF},
    {0x380e, (984 >> 8) & 0xFF},
    {0x380f, 984 & 0xFF},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3708, 0x64},
    {0x3709, 0x52},
    {0x3800, (500 >> 8) & 0x0F},
    {0x3801, 500 & 0xFF},
    {0x3802, 0x00},
    {0x3803, 0x00},
    {0x3804, ((2624 - 1) >> 8) & 0x0F},
    {0x3805, (2624 - 1) & 0xFF},
    {0x3806, ((1954 - 1) >> 8) & 0x07},
    {0x3807, (1954 - 1) & 0xFF},
    {0x3808, (800 >> 8) & 0x0F},
    {0x3809, 800 & 0xFF},
    {0x380a, (640 >> 8) & 0x7F},
    {0x380b, 640 & 0xFF},
    {0x3810, 0x00},
    {0x3811, 8},
    {0x3812, 0x00},
    {0x3813, 0x00},
    {0x3630, 0x2e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3634, 0x44},
    {0x3636, 0x06},
    {0x3620, 0x64},
    {0x3621, 0xe0},
    {0x3600, 0x37},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x3731, 0x02},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3f05, 0x02},
    {0x3f06, 0x10},
    {0x3f01, 0x0a},
    {0x3a08, 0x01},
    {0x3a09, 0x27},
    {0x3a0a, 0x00},
    {0x3a0b, 0xf6},
    {0x3a0d, 0x04},
    {0x3a0e, 0x03},
    {0x3a0f, 0x58},
    {0x3a10, 0x50},
    {0x3a1b, 0x58},
    {0x3a1e, 0x50},
    {0x3a11, 0x60},
    {0x3a1f, 0x28},
    {0x4001, 0x02},
    {0x4004, 0x02},
    {0x4000, 0x09},
    {0x4837, (uint8_t)(1000000000 / (OV5647_IDI_CLOCK_RATE_800x640_50FPS / 4))},
    {0x4050, 0x6e},
    {0x4051, 0x8f},
    {OV5647_REG_END, 0x00},
};

/* Espressif ov5647_mipi_2lane_24Minput_1920x1080_raw10_30fps */
static const ov5647_reginfo_t ov5647_1920x1080_raw10[] = {
    {0x3034, OV5647_10BIT_MODE},
    {0x3035, 0x21},
    {0x3036, 0x62},
    {0x303c, 0x11},
    {0x3106, 0xf5},
    {0x3821, 0x02},
    {0x3820, 0x00},
    {0x3827, 0xec},
    {0x370c, 0x03},
    {0x3612, 0x5b},
    {0x3618, 0x04},
    {0x5000, 0xff},
    {0x583e, 0xf0},
    {0x583f, 0x4f},
    {0x5003, 0x08},
    {0x5a00, 0x08},
    {0x3000, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3016, 0x08},
    {0x3017, 0xe0},
    {0x3018, 0x44},
    {0x301c, 0xf8},
    {0x301d, 0xf0},
    {0x3a18, 0x03},
    {0x3a19, 0xff},
    {0x3c00, 0x40},
    {0x3b07, 0x0c},
    {0x380c, 0x08},
    {0x380d, 0xdf},
    {0x380e, 0x04},
    {0x380f, 0xaf},
    {0x3814, 0x11},
    {0x3815, 0x11},
    {0x3708, 0x64},
    {0x3709, 0x12},
    {0x3808, 0x07},
    {0x3809, 0x80},
    {0x380a, 0x04},
    {0x380b, 0x38},
    {0x3800, 0x01},
    {0x3801, 0x5c},
    {0x3802, 0x01},
    {0x3803, 0xb2},
    {0x3804, 0x08},
    {0x3805, 0xe3},
    {0x3806, 0x05},
    {0x3807, 0xf1},
    {0x3811, 0x04},
    {0x3813, 0x02},
    {0x3630, 0x2e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3634, 0x44},
    {0x3636, 0x06},
    {0x3620, 0x64},
    {0x3621, 0xe0},
    {0x3600, 0x37},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x3731, 0x02},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3f05, 0x02},
    {0x3f06, 0x10},
    {0x3f01, 0x0a},
    {0x3a08, 0x01},
    {0x3a09, 0x4b},
    {0x3a0a, 0x01},
    {0x3a0b, 0x13},
    {0x3a0d, 0x04},
    {0x3a0e, 0x03},
    {0x3a0f, 0x58},
    {0x3a10, 0x50},
    {0x3a1b, 0x58},
    {0x3a1e, 0x50},
    {0x3a11, 0x60},
    {0x3a1f, 0x28},
    {0x4001, 0x02},
    {0x4004, 0x04},
    {0x4000, 0x09},
    {0x4837, 0x19},
    {0x4800, 0x34},
    {OV5647_REG_END, 0x00},
};

static bool write_table(uint8_t addr7, const ov5647_reginfo_t *t) {
  for (size_t i = 0; t[i].reg != OV5647_REG_END; i++) {
    if (t[i].reg == OV5647_REG_DELAY) {
      delay(t[i].val ? t[i].val : 10);
      continue;
    }
    if (!imx708_i2c_write8(addr7, t[i].reg, t[i].val)) return false;
  }
  return true;
}

bool ov5647_detect(uint8_t *addr7_out) {
  uint8_t hi = 0, lo = 0;
  if (!imx708_i2c_read8(OV5647_ADDR, 0x300A, &hi)) return false;
  if (!imx708_i2c_read8(OV5647_ADDR, 0x300B, &lo)) return false;
  uint16_t id = ((uint16_t)hi << 8) | lo;
  if (id != 0x5647) {
    Serial.printf("CSI: 0x36 present but id=0x%04X (not OV5647)\n", id);
    return false;
  }
  *addr7_out = OV5647_ADDR;
  return true;
}

bool ov5647_configure_800x640_raw8(uint8_t addr7) {
  if (!write_table(addr7, ov5647_reset)) return false;
  if (!write_table(addr7, ov5647_800x640_raw8)) return false;
  if (!imx708_i2c_write8(addr7, 0x4800, 0x14)) return false;
  if (!imx708_i2c_write8(addr7, 0x4202, 0x00)) return false;
  if (!imx708_i2c_write8(addr7, 0x300D, 0x00)) return false;
  if (!imx708_i2c_write8(addr7, 0x3503, 0x00)) return false;
  s_exp_max_lines = 980;
  return imx708_i2c_write8(addr7, 0x0100, 0x00);
}

bool ov5647_configure_1920x1080_raw10(uint8_t addr7) {
  if (!write_table(addr7, ov5647_reset)) return false;
  if (!write_table(addr7, ov5647_1920x1080_raw10)) return false;
  if (!imx708_i2c_write8(addr7, 0x4202, 0x00)) return false;
  if (!imx708_i2c_write8(addr7, 0x300D, 0x00)) return false;
  if (!imx708_i2c_write8(addr7, 0x3503, 0x00)) return false;
  s_exp_max_lines = 1190;  // VTS=1199
  return imx708_i2c_write8(addr7, 0x0100, 0x00);
}

bool ov5647_stream_restart(uint8_t addr7) {
  if (!imx708_i2c_write8(addr7, 0x0100, 0x00)) return false;
  if (!imx708_i2c_write8(addr7, 0x4800, 0x14)) return false;
  delay(5);
  return imx708_i2c_write8(addr7, 0x0100, 0x01);
}

bool ov5647_set_test_pattern(uint8_t addr7, bool enable) {
  return imx708_i2c_write8(addr7, 0x503D, enable ? 0x80 : 0x00);
}

static bool update_bit(uint8_t addr7, uint16_t reg, uint8_t bit, bool set) {
  uint8_t v = 0;
  if (!imx708_i2c_read8(addr7, reg, &v)) return false;
  if (set) v |= (uint8_t)(1u << bit);
  else v = (uint8_t)(v & ~(1u << bit));
  return imx708_i2c_write8(addr7, reg, v);
}

bool ov5647_set_hmirror(uint8_t addr7, bool enable) {
  /* Bit1 = sensor mirror. Built-in optical path is mirrored; match Linux driver. */
  return update_bit(addr7, 0x3821, 1, !enable);
}

bool ov5647_set_vflip(uint8_t addr7, bool enable) {
  return update_bit(addr7, 0x3820, 1, enable);
}

bool ov5647_get_hmirror(uint8_t addr7, bool *out) {
  uint8_t v = 0;
  if (!out || !imx708_i2c_read8(addr7, 0x3821, &v)) return false;
  *out = !(v & 0x02);
  return true;
}

bool ov5647_get_vflip(uint8_t addr7, bool *out) {
  uint8_t v = 0;
  if (!out || !imx708_i2c_read8(addr7, 0x3820, &v)) return false;
  *out = (v & 0x02) != 0;
  return true;
}

bool ov5647_set_aec(uint8_t addr7, bool enable) {
  /* Bit0: 0=auto AEC, 1=manual */
  return update_bit(addr7, 0x3503, 0, !enable);
}

bool ov5647_set_agc(uint8_t addr7, bool enable) {
  /* Bit1: 0=auto AGC, 1=manual */
  return update_bit(addr7, 0x3503, 1, !enable);
}

bool ov5647_get_aec(uint8_t addr7, bool *out) {
  uint8_t v = 0;
  if (!out || !imx708_i2c_read8(addr7, 0x3503, &v)) return false;
  *out = !(v & 0x01);
  return true;
}

bool ov5647_get_agc(uint8_t addr7, bool *out) {
  uint8_t v = 0;
  if (!out || !imx708_i2c_read8(addr7, 0x3503, &v)) return false;
  *out = !(v & 0x02);
  return true;
}

/* Clamped to current mode VTS (set by configure_*). */
uint16_t ov5647_exposure_max_lines(void) { return s_exp_max_lines; }

static bool group_hold_begin(uint8_t addr7) {
  return imx708_i2c_write8(addr7, 0x3208, 0x00);
}

static bool group_hold_launch(uint8_t addr7) {
  if (!imx708_i2c_write8(addr7, 0x3208, 0x10)) return false;  // end group 0
  return imx708_i2c_write8(addr7, 0x3208, 0xa0);              // launch
}

bool ov5647_set_exposure(uint8_t addr7, uint16_t lines) {
  if (lines < 4) lines = 4;
  if (lines > ov5647_exposure_max_lines()) lines = ov5647_exposure_max_lines();
  uint32_t raw = ((uint32_t)lines) << 4;
  if (raw > 0xFFFFF) raw = 0xFFFFF;
  // Group-hold so 0x3500..02 update as one frame — avoids dark mid-write flicker.
  if (!group_hold_begin(addr7)) return false;
  if (!imx708_i2c_write8(addr7, 0x3500, (uint8_t)((raw >> 16) & 0x0F))) return false;
  if (!imx708_i2c_write8(addr7, 0x3501, (uint8_t)((raw >> 8) & 0xFF))) return false;
  if (!imx708_i2c_write8(addr7, 0x3502, (uint8_t)(raw & 0xFF))) return false;
  return group_hold_launch(addr7);
}

bool ov5647_get_exposure(uint8_t addr7, uint16_t *lines) {
  uint8_t b0 = 0, b1 = 0, b2 = 0;
  if (!lines) return false;
  if (!imx708_i2c_read8(addr7, 0x3500, &b0)) return false;
  if (!imx708_i2c_read8(addr7, 0x3501, &b1)) return false;
  if (!imx708_i2c_read8(addr7, 0x3502, &b2)) return false;
  uint32_t raw = ((uint32_t)(b0 & 0x0F) << 16) | ((uint32_t)b1 << 8) | b2;
  *lines = (uint16_t)(raw >> 4);
  return true;
}

bool ov5647_set_gain(uint8_t addr7, uint16_t gain) {
  if (gain > 0x3FF) gain = 0x3FF;
  if (!group_hold_begin(addr7)) return false;
  if (!imx708_i2c_write8(addr7, 0x350A, (uint8_t)((gain >> 8) & 0x03))) return false;
  if (!imx708_i2c_write8(addr7, 0x350B, (uint8_t)(gain & 0xFF))) return false;
  return group_hold_launch(addr7);
}

bool ov5647_get_gain(uint8_t addr7, uint16_t *gain) {
  uint8_t hi = 0, lo = 0;
  if (!gain) return false;
  if (!imx708_i2c_read8(addr7, 0x350A, &hi)) return false;
  if (!imx708_i2c_read8(addr7, 0x350B, &lo)) return false;
  *gain = (uint16_t)(((hi & 0x03) << 8) | lo);
  return true;
}

bool ov5647_set_gainceiling(uint8_t addr7, uint16_t ceiling) {
  if (!imx708_i2c_write8(addr7, 0x3A18, (uint8_t)((ceiling >> 8) & 0xFF))) return false;
  return imx708_i2c_write8(addr7, 0x3A19, (uint8_t)(ceiling & 0xFF));
}

bool ov5647_get_gainceiling(uint8_t addr7, uint16_t *ceiling) {
  uint8_t hi = 0, lo = 0;
  if (!ceiling) return false;
  if (!imx708_i2c_read8(addr7, 0x3A18, &hi)) return false;
  if (!imx708_i2c_read8(addr7, 0x3A19, &lo)) return false;
  *ceiling = (uint16_t)((hi << 8) | lo);
  return true;
}

void ov5647_dump_key_regs(uint8_t addr7) {
  static const struct {
    uint16_t reg;
    const char *name;
  } regs[] = {
      {0x0100, "stream"},   {0x4800, "mipi"},    {0x3018, "lanes"},
      {0x3034, "fmt"},      {0x3036, "pll"},     {0x503D, "testpat"},
      {0x3503, "aec"},
  };
  for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
    uint8_t v = 0;
    if (imx708_i2c_read8(addr7, regs[i].reg, &v)) {
      Serial.printf("CSI: OV5647 0x%04X (%s) = 0x%02X\n", regs[i].reg, regs[i].name, v);
    } else {
      Serial.printf("CSI: OV5647 0x%04X (%s) read fail\n", regs[i].reg, regs[i].name);
    }
  }
}
