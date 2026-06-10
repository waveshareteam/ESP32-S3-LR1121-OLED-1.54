#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "main";

#define RTC_ALARM_IO_NUM    7
#define RTC_ALARM_ACTIVE_LEVEL  0

pcf85063a_datetime_t rtc_time = {
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
    bsp_rtc_init();
    bsp_set_rtc_time_date(rtc_time);
    rtc_time.sec = 29;
    bsp_set_rtc_alarm_time(rtc_time);
    bsp_enable_rtc_alarm();

    const button_config_t btn_cfg = {0};
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = RTC_ALARM_IO_NUM,
        .active_level = RTC_ALARM_ACTIVE_LEVEL,
    };

    button_handle_t btn = NULL;
    esp_err_t ret;

    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    if (ret != ESP_OK) return;
    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, NULL);

    while (1) {
        bsp_get_rtc_time_date(&rtc_time);
        char datetime_str[32];
        bsp_datetime_to_str(datetime_str, rtc_time);
        ESP_LOGI(TAG, "RTC time: %s", datetime_str);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
