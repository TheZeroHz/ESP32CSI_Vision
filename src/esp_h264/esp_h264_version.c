/* Arduino packaging: HW encoder is ESP32-P4 only. */
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32P4

/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_h264_version.h"

const char *esp_h264_get_version(void)
{
    return ESP_H264_VERSION;
}

#endif /* CONFIG_IDF_TARGET_ESP32P4 */
