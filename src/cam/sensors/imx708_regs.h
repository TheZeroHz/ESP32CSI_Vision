/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Special register values */
#define IMX708_REG_DELAY            0xeeee
#define IMX708_REG_END              0xffff

/* Chip ID */
#define IMX708_REG_CHIP_ID          0x0016
#define IMX708_CHIP_ID              0x0708

/* Mode Select */
#define IMX708_REG_MODE_SELECT      0x0100
#define IMX708_MODE_STANDBY         0x00
#define IMX708_MODE_STREAMING       0x01

/* Orientation */
#define IMX708_REG_ORIENTATION      0x0101

/* Frame length */
#define IMX708_REG_FRAME_LENGTH     0x0340
#define IMX708_FRAME_LENGTH_MAX     0xffff

/* Exposure control */
#define IMX708_REG_EXPOSURE         0x0202
#define IMX708_EXPOSURE_OFFSET      48
// Default exposure at power-up (in sensor lines). For 15 fps, VTS=3296, 1 line ≈ 20.24 us
// 173 lines ≈ 3.5 ms, which avoids underexposed starts in cloudy outdoor scenes
#define IMX708_EXPOSURE_DEFAULT     173
#define IMX708_EXPOSURE_STEP        1
#define IMX708_EXPOSURE_MIN         1
#define IMX708_EXPOSURE_MAX         (IMX708_FRAME_LENGTH_MAX - IMX708_EXPOSURE_OFFSET)

/* Analog gain control */
#define IMX708_REG_ANALOG_GAIN      0x0204
#define IMX708_ANA_GAIN_MIN         112
#define IMX708_ANA_GAIN_MAX         960
#define IMX708_ANA_GAIN_STEP        1
#define IMX708_ANA_GAIN_DEFAULT     IMX708_ANA_GAIN_MIN

/* Digital gain control */
#define IMX708_REG_DIGITAL_GAIN     0x020e
#define IMX708_DGTL_GAIN_MIN        0x0100
#define IMX708_DGTL_GAIN_MAX        0xffff
#define IMX708_DGTL_GAIN_DEFAULT    0x0100
#define IMX708_DGTL_GAIN_STEP       1

/* Color balance controls */
#define IMX708_REG_COLOUR_BALANCE_RED   0x0b90
#define IMX708_REG_COLOUR_BALANCE_BLUE  0x0b92
#define IMX708_COLOUR_BALANCE_MIN       0x01
#define IMX708_COLOUR_BALANCE_MAX       0xffff
#define IMX708_COLOUR_BALANCE_STEP      0x01
#define IMX708_COLOUR_BALANCE_DEFAULT   0x100

/* Test Pattern Control */
#define IMX708_REG_TEST_PATTERN     0x0600
#define IMX708_TEST_PATTERN_DISABLE 0
#define IMX708_TEST_PATTERN_SOLID_COLOR  1
#define IMX708_TEST_PATTERN_COLOR_BARS   2
#define IMX708_TEST_PATTERN_GREY_COLOR   3
#define IMX708_TEST_PATTERN_PN9          4

/* Test pattern color components */
#define IMX708_REG_TEST_PATTERN_R   0x0602
#define IMX708_REG_TEST_PATTERN_GR  0x0604
#define IMX708_REG_TEST_PATTERN_B   0x0606
#define IMX708_REG_TEST_PATTERN_GB  0x0608
#define IMX708_TEST_PATTERN_COLOUR_MIN   0
#define IMX708_TEST_PATTERN_COLOUR_MAX   0x0fff
#define IMX708_TEST_PATTERN_COLOUR_STEP  1

/* HDR exposure controls */
#define IMX708_HDR_EXPOSURE_RATIO   4
#define IMX708_REG_MID_EXPOSURE     0x3116
#define IMX708_REG_SHT_EXPOSURE     0x0224
#define IMX708_REG_MID_ANALOG_GAIN  0x3118
#define IMX708_REG_SHT_ANALOG_GAIN  0x0216

/* Long exposure multiplier */
#define IMX708_LONG_EXP_SHIFT_MAX   7
#define IMX708_LONG_EXP_SHIFT_REG   0x3100

/* QBC Re-mosaic broken line correction registers */
#define IMX708_LPF_INTENSITY_EN     0xC428
#define IMX708_LPF_INTENSITY_ENABLED    0x00
#define IMX708_LPF_INTENSITY_DISABLED   0x01
#define IMX708_LPF_INTENSITY        0xC429

/* PDAF gains base addresses */
#define IMX708_REG_BASE_SPC_GAINS_L 0x7b10
#define IMX708_REG_BASE_SPC_GAINS_R 0x7c00

/* Clock settings */
#define IMX708_INCLK_FREQ           24000000

/* Pixel array dimensions */
#define IMX708_NATIVE_WIDTH         4640U
#define IMX708_NATIVE_HEIGHT        2658U
#define IMX708_PIXEL_ARRAY_LEFT     16U
#define IMX708_PIXEL_ARRAY_TOP      24U
#define IMX708_PIXEL_ARRAY_WIDTH    4608U
#define IMX708_PIXEL_ARRAY_HEIGHT   2592U

/* Link frequency registers */
#define IMX708_REG_PLL_CTRL_0E      0x030E
#define IMX708_REG_PLL_CTRL_0F      0x030F

#ifdef __cplusplus
}
#endif
