/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_log.h"
#include "board_support.h"
#include "button_gpio.h"
#include "iot_button.h"

static const char *TAG = "button_main";


static void button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    int button_id = (int)(intptr_t)data;

    ESP_LOGI(TAG, "%s :%d", iot_button_get_event_str(event), button_id);

    if (BUTTON_PRESS_REPEAT == event || BUTTON_PRESS_REPEAT_DONE == event) {
        ESP_LOGI(TAG, "\tREPEAT[%d]", iot_button_get_repeat(arg));
    }

    if (BUTTON_PRESS_UP == event || BUTTON_LONG_PRESS_HOLD == event || BUTTON_LONG_PRESS_UP == event) {
        ESP_LOGI(TAG, "\tPressed Time[%" PRIu32 "]", iot_button_get_pressed_time(arg));
    }

    if (BUTTON_MULTIPLE_CLICK == event) {
        ESP_LOGI(TAG, "\tMULTIPLE[%d]", button_id);
    }
}

static esp_err_t register_button(int gpio_num, bool double_click)
{
    button_handle_t btn = board_button_for_gpio((gpio_num_t)gpio_num);
    if (!btn) return ESP_FAIL;
    esp_err_t ret;
    ret = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
    if (ret != ESP_OK) {
        return ret;
    }
    if (double_click) {
        ret = iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

void app_main(void)
{
    const struct {
        int gpio_num;
        bool double_click;
    } buttons[] = {
        {BOOT_KEY_GPIO, false},
        {MENU_KEY_GPIO, false},
        {PTT_KEY_GPIO, true},
        {PWR_KEY_GPIO, false},
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        esp_err_t ret = register_button(buttons[i].gpio_num, buttons[i].double_click);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register GPIO %d button: %s", buttons[i].gpio_num, esp_err_to_name(ret));
            return;
        }
    }

    ESP_LOGI(TAG, "System ready, waiting key input...");
}
