/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_support.h"

static const char *TAG = "rtc_pcf85063";


static pcf85063a_datetime_t rtc_time = {
    .year = 2026,
    .month = 04,
    .day = 16,
    .dotw = 4,
    .hour = 14,
    .min = 49,
    .sec = 26,
};

static void button_event_cb(void *arg, void *data)
{
    (void)data;

    button_event_t event = iot_button_get_event(arg);
    if (BUTTON_PRESS_DOWN == event) {
        ESP_LOGI(TAG, "Alarm flag pressed");
    }
}

void app_main(void)
{
    esp_err_t ret = board_rtc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RTC: %s", esp_err_to_name(ret));
        return;
    }

    ret = board_set_rtc_time_date(rtc_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RTC time: %s", esp_err_to_name(ret));
        return;
    }

    rtc_time.sec = 29;
    ret = board_set_rtc_alarm_time(rtc_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RTC alarm: %s", esp_err_to_name(ret));
        return;
    }

    ret = board_enable_rtc_alarm();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RTC alarm: %s", esp_err_to_name(ret));
        return;
    }

    button_handle_t btn = board_button("button_rtc_irq");
    if (!btn) return;

    ret = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register RTC alarm callback: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        ret = board_get_rtc_time_date(&rtc_time);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read RTC time: %s", esp_err_to_name(ret));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        char datetime_str[32];
        board_datetime_to_str(datetime_str, rtc_time);
        ESP_LOGI(TAG, "RTC time: %s", datetime_str);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
