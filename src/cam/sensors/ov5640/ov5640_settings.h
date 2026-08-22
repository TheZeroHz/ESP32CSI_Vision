/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Arduino MIPI-only slice of Espressif ov5640_settings.h (no DVP).
 */
#pragma once

#include "ov5640_regs.h"
#include "ov5640_types.h"

#ifndef CONFIG_CAMERA_OV5640_CSI_LINESYNC_ENABLE
#define CONFIG_CAMERA_OV5640_CSI_LINESYNC_ENABLE 0
#endif

#define OV5640_SOFT_POWER_DOWN_EN (0x42)
#define OV5640_SOFT_POWER_DOWN_DIS (0x02)
#define OV5640_IDI_CLOCK_RATE_1280x720_14FPS (80000000ULL)

#define ov5640_settings_mipi_rgb565_le \
  {FORMAT_CTRL0, 0x6f}, {FORMAT_MUX_CTRL, 0x01}

#include "ov5640_mipi_2lane_24Minput_1280x720_rgb565_le_14fps.h"
