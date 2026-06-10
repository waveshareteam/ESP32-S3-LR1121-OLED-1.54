#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_log.h"
#include "button_gpio.h"
#include "iot_button.h"

static const char *TAG = "main";

#define GPOI0_IO_NUM 0
#define GPOI4_IO_NUM 4
#define GPOI5_IO_NUM 5
#define GPOI6_IO_NUM 6
#define BUTTON_ACTIVE_LEVEL 0

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
    const button_config_t btn_cfg = {0};
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = gpio_num,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };
    button_handle_t btn = NULL;

    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    if (ret != ESP_OK) {
        return ret;
    }

    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
    if (double_click) {
        iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
    }

    return ESP_OK;
}

void app_main(void)
{
    const struct {
        int gpio_num;
        bool double_click;
    } buttons[] = {
        {GPOI0_IO_NUM, false},
        {GPOI5_IO_NUM, false},
        {GPOI6_IO_NUM, true},
        {GPOI4_IO_NUM, false},
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        if (register_button(buttons[i].gpio_num, buttons[i].double_click) != ESP_OK) {
            return;
        }
    }

    ESP_LOGI(TAG, "System ready, waiting key input...");
}
