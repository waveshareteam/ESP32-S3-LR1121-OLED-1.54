// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_board_manager.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "sdmmc_cmd.h"
#include "iot_button.h"
#include "pcf85063a.h"
#include "board_hardware.h"

// Keep dependency headers outside extern "C": ESP-IDF LCD APIs have C++ overloads.
#if CONFIG_WS_BOARD_AUDIO
#include "esp_codec_dev.h"
#endif
#if CONFIG_WS_BOARD_DISPLAY
#include "esp_lv_adapter.h"
#include "lvgl.h"
#if LVGL_VERSION_MAJOR != 9
#error "This product requires LVGL_VERSION_MAJOR == 9"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Board Manager owns buses/devices. Initialize devices lazily so an absent SD
// card or radio never prevents an unrelated example from starting.
esp_err_t board_init(void);
esp_err_t board_peripheral(const char *name, void **handle);
esp_err_t board_device(const char *name, void **handle);
button_handle_t board_button(const char *name);
button_handle_t board_button_for_gpio(gpio_num_t gpio);
int board_button_active_level(gpio_num_t gpio);
esp_err_t board_i2c_init(void);
i2c_master_bus_handle_t board_i2c_get_handle(void);
esp_err_t board_spi_init(void);
spi_device_handle_t board_spi_get_handle(void);
#define BSP_SD_MOUNT_POINT "/sdcard"
extern sdmmc_card_t *bsp_sdcard;
esp_err_t board_sdcard_mount(void);
esp_err_t board_sdcard_unmount(void);
esp_err_t board_audio_poweramp_enable(bool enable);
esp_err_t board_rtc_init(void);
esp_err_t board_set_rtc_time_date(pcf85063a_datetime_t time);
esp_err_t board_set_rtc_alarm_time(pcf85063a_datetime_t time);
esp_err_t board_get_rtc_time_date(pcf85063a_datetime_t *time);
esp_err_t board_get_rtc_alarm_time(pcf85063a_datetime_t *time);
esp_err_t board_enable_rtc_alarm(void);
esp_err_t board_datetime_to_str(char *str, pcf85063a_datetime_t time);
typedef struct {
    uint16_t mv;        // 电压（mV）
    int16_t ma;         // 电流（mA），正值=充电，负值=放电
    int16_t max_load;   // 负载电流（mA）
    int16_t mw;         // 功率（mW）
    uint16_t soc;       // 剩余电量百分比（%）
    uint16_t tte;       // 预计还能用多久（min）
    uint16_t ttf;       // 预计充满还需多久（min）
    uint16_t mah_rem;   // 剩余容量（mAh）
    uint16_t mah_fcc;   // 满充容量（mAh）
    uint16_t cycles;    // 循环次数（cycle count）
    uint16_t soh;       // 健康度（State of Health %）
    uint16_t tc;        // 温度（°C）
    bool charging;      // 电池当前是否有充电电流
} bsp_bat_info_t;

esp_err_t board_bat_init(uint16_t mah);
esp_err_t board_get_bat_info(bsp_bat_info_t *info);
esp_err_t board_update_bat_charge_status(bsp_bat_info_t *info);
#if CONFIG_WS_BOARD_AUDIO
esp_codec_dev_handle_t board_audio_codec_speaker_init(uint32_t sample_rate);
esp_codec_dev_handle_t board_audio_codec_microphone_init(uint32_t sample_rate);
#endif
#if CONFIG_WS_BOARD_DISPLAY
lv_display_t *board_display_start(void);
lv_indev_t *board_display_get_input_dev(void);
bool board_display_lock(int32_t timeout_ms);
void board_display_unlock(void);
esp_err_t board_display_backlight_on(void);
esp_err_t board_display_backlight_off(void);
#endif
#ifdef __cplusplus
}
#endif
