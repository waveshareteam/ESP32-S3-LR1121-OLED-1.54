/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "rtc_pcf85063";

#define RTC_ALARM_IO_NUM    7
#define RTC_ALARM_ACTIVE_LEVEL  0

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
    esp_err_t ret = bsp_rtc_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RTC: %s", esp_err_to_name(ret));
        return;
    }

    ret = bsp_set_rtc_time_date(rtc_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RTC time: %s", esp_err_to_name(ret));
        return;
    }

    rtc_time.sec = 29;
    ret = bsp_set_rtc_alarm_time(rtc_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RTC alarm: %s", esp_err_to_name(ret));
        return;
    }

    ret = bsp_enable_rtc_alarm();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RTC alarm: %s", esp_err_to_name(ret));
        return;
    }

    const button_config_t btn_cfg = {0};
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = RTC_ALARM_IO_NUM,
        .active_level = RTC_ALARM_ACTIVE_LEVEL,
    };

    button_handle_t btn = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RTC alarm button: %s", esp_err_to_name(ret));
        return;
    }

    ret = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register RTC alarm callback: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        ret = bsp_get_rtc_time_date(&rtc_time);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read RTC time: %s", esp_err_to_name(ret));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        char datetime_str[32];
        bsp_datetime_to_str(datetime_str, rtc_time);
        ESP_LOGI(TAG, "RTC time: %s", datetime_str);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
