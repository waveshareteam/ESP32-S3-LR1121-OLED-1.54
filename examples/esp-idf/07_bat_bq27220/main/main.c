#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "main";

void app_main(void)
{
    bsp_bat_info_t bat_info;
    bsp_bat_init(1000);

    while (1) {
        bsp_get_bat_info(&bat_info);
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
