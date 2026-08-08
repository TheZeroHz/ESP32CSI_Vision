/**
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 * Arduino packaging: force P4 rev>=300 HAL paths to match hw_ver3 register
 * structs and Arduino-ESP32 3.3.x (CONFIG_ESP_REV_MIN_FULL=301).
 */
#pragma once

#include "sdkconfig.h"

#ifndef HAL_CONFIG
#define HAL_CONFIG(x) HAL_CONFIG_##x
#endif

/* Prefer IDF value when present; otherwise assume P4 eco / rev 301. */
#ifdef CONFIG_ESP_REV_MIN_FULL
#define HAL_CONFIG_CHIP_SUPPORT_MIN_REV CONFIG_ESP_REV_MIN_FULL
#else
#define HAL_CONFIG_CHIP_SUPPORT_MIN_REV 301
#endif

#if HAL_CONFIG(CHIP_SUPPORT_MIN_REV) < 300
/* Arduino-ESP32 P4 libs are built for rev 301 + hw_ver3 structs. */
#undef HAL_CONFIG_CHIP_SUPPORT_MIN_REV
#define HAL_CONFIG_CHIP_SUPPORT_MIN_REV 301
#endif
