/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_support.h"

static const char *TAG = "bat_bq27220";

void app_main(void)
{
    bsp_bat_info_t bat_info;
    esp_err_t ret = board_bat_init(1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize battery monitor: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        ret = board_get_bat_info(&bat_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read battery info: %s", esp_err_to_name(ret));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "[BAT] %dmV %dmA SOC:%d%% REM:%dmAH FCC:%dmAH TMP:%d C",
                 bat_info.mv,
                 bat_info.ma,
                 bat_info.soc,
                 bat_info.mah_rem,
                 bat_info.mah_fcc,
                 bat_info.tc);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
