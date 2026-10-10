// SPDX-License-Identifier: Apache-2.0
// Product glue derived from the Waveshare BSP; chip drivers remain managed.
#include "board_support.h"
#include "esp_check.h"
#include "esp_board_entry.h"
#include "gen_board_device_custom.h"
#include "bq27220.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "ws_sensors";
#define BSP_ERROR_CHECK_RETURN_ERR(x) do { esp_err_t e = (x); if (e != ESP_OK) return e; } while (0)
static pcf85063a_dev_t dev;
static bq27220_handle_t bq27220;
static i2c_bus_handle_t i2c_bus;
static const dev_custom_battery_config_t *battery_config;
static uint16_t battery_capacity_mah;
static int rtc_init(void *config, int size, void **handle)
{
    if (size != sizeof(dev_custom_rtc_config_t)) return ESP_ERR_INVALID_ARG;
    const dev_custom_rtc_config_t *cfg = config;
    i2c_master_bus_handle_t bus = board_i2c_get_handle();
    if (!bus) return ESP_FAIL;
    ESP_RETURN_ON_ERROR(pcf85063a_init(&dev, bus, cfg->address), TAG, "RTC driver");
    *handle = &dev;
    return ESP_OK;
}
// These devices live for the lifetime of the firmware; no application releases them.
static int sensor_deinit(void *handle) { (void)handle; return ESP_ERR_NOT_SUPPORTED; }
ESP_BOARD_ENTRY_IMPLEMENT(rtc, rtc_init, sensor_deinit);
esp_err_t board_rtc_init(void)
{
    void *handle;
    return board_device("rtc", &handle);
}
esp_err_t board_set_rtc_time_date(pcf85063a_datetime_t time)
{
    esp_err_t ret = pcf85063a_set_time_date(&dev, time);
    return ret;
}

esp_err_t board_set_rtc_alarm_time(pcf85063a_datetime_t time)
{
    esp_err_t ret = pcf85063a_set_alarm(&dev, time);
    return ret;
}

esp_err_t board_get_rtc_time_date(pcf85063a_datetime_t *time)
{
    esp_err_t ret = pcf85063a_get_time_date(&dev, time);
    return ret;
}

esp_err_t board_get_rtc_alarm_time(pcf85063a_datetime_t *time)
{
    esp_err_t ret = pcf85063a_get_alarm(&dev, time);
    return ret;
}

esp_err_t board_enable_rtc_alarm()
{
    esp_err_t ret = pcf85063a_enable_alarm(&dev);
    return ret;
}

esp_err_t board_datetime_to_str(char *datetime_str, pcf85063a_datetime_t time)
{
    pcf85063a_datetime_to_str(datetime_str, 32, time);
    return ESP_OK;
}

static esp_err_t create_battery(uint16_t mah)
{
    static parameter_cedv_t default_cedv = {
        .full_charge_cap = 650,
        .design_cap = 650,
        .reserve_cap = 0,
        .near_full = 200,
        .self_discharge_rate = 20,
        .EDV0 = 3490,
        .EDV1 = 3511,
        .EDV2 = 3535,
        .EMF = 3670,
        .C0 = 115,
        .R0 = 968,
        .T0 = 4547,
        .R1 = 4764,
        .TC = 11,
        .C1 = 0,
        .DOD0 = 4147,
        .DOD10 = 4002,
        .DOD20 = 3969,
        .DOD30 = 3938,
        .DOD40 = 3880,
        .DOD50 = 3824,
        .DOD60 = 3794,
        .DOD70 = 3753,
        .DOD80 = 3677,
        .DOD90 = 3574,
        .DOD100 = 3490,
    };
    default_cedv.full_charge_cap = mah;
    default_cedv.design_cap = mah;
    static const gauging_config_t default_config = {
        .CCT = 1,
        .CSYNC = 0,
        .EDV_CMP = 0,
        .SC = 1,
        .FIXED_EDV0 = 0,
        .FCC_LIM = 1,
        .FC_FOR_VDQ = 1,
        .IGNORE_SD = 1,
        .SME0 = 0,
    };

    bq27220_config_t bq27220_cfg = {
        .i2c_bus = i2c_bus,
        .cfg = &default_config,
        .cedv = &default_cedv,
    };

    bq27220 = bq27220_create(&bq27220_cfg);
    return bq27220 ? ESP_OK : ESP_FAIL;
}

esp_err_t board_get_bat_info(bsp_bat_info_t *bat_info)
{
    ESP_RETURN_ON_FALSE(bat_info && bq27220, ESP_ERR_INVALID_STATE, TAG, "battery not initialized");
    bat_info->mv  = bq27220_get_voltage(bq27220);
    BSP_ERROR_CHECK_RETURN_ERR(board_update_bat_charge_status(bat_info));
    bat_info->mah_rem = bq27220_get_remaining_capacity(bq27220);
    bat_info->mah_fcc = bq27220_get_full_charge_capacity(bq27220);
    bat_info->tc = bq27220_get_temperature(bq27220) / 10 - 273; // Convert from 0.1K to Celsius
    bat_info->cycles = bq27220_get_cycle_count(bq27220);
    bat_info->soc = bq27220_get_state_of_charge(bq27220);
    bat_info->mw = bq27220_get_average_power(bq27220); // in mW
    bat_info->max_load = bq27220_get_maxload_current(bq27220); // in mA
    bat_info->tte = bq27220_get_time_to_empty(bq27220);
    bat_info->ttf = bq27220_get_time_to_full(bq27220);
    bat_info->soh = bq27220_get_state_of_health(bq27220);

    // ESP_LOGI(TAG, "Battery Info - Vol: %dmv, Current: %dmA, Power: %dmW, Remaining Capacity: %dmAh, Full Charge Capacity: %dmAh, Temperature: %dC, Cycle Count: %d, SOC: %d%%, Max Load: %dmA, Time to empty: %dmin, Time to full: %dmin, SOH=%d%%",
    //          bat_info->mv, bat_info->ma, bat_info->mw, bat_info->mah_rem, bat_info->mah_fcc, bat_info->tc, bat_info->cycles, bat_info->soc, bat_info->max_load, bat_info->tte, bat_info->ttf, bat_info->soh);

    return ESP_OK;
}

esp_err_t board_update_bat_charge_status(bsp_bat_info_t *bat_info)
{
    ESP_RETURN_ON_FALSE(bat_info && bq27220, ESP_ERR_INVALID_ARG, TAG, "Invalid battery handle");

    int16_t current_ma = bq27220_get_current(bq27220);
    bat_info->ma = current_ma;
    bat_info->charging = current_ma >= battery_config->charge_threshold_ma;

    static TickType_t last_log_tick = 0;
    TickType_t now = xTaskGetTickCount();
    if (now - last_log_tick >= pdMS_TO_TICKS(1000)) {
        last_log_tick = now;
        // ESP_LOGI(TAG, "BQ27220 current: %dmA, charging: %d", current_ma, bat_info->charging);
    }

    return ESP_OK;
}


static int battery_init(void *config, int size, void **handle)
{
    if (size != sizeof(dev_custom_battery_config_t)) return ESP_ERR_INVALID_ARG;
    battery_config = config;
    ESP_RETURN_ON_ERROR(board_init(), TAG, "battery bus");
    i2c_master_bus_config_t *bus_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_periph_config("i2c_main", (void **)&bus_cfg), TAG, "bus config");
    // i2c_bus 1.4.2 V2 detects and borrows Board Manager's existing bus.
    const i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER, .sda_io_num = bus_cfg->sda_io_num,
        .scl_io_num = bus_cfg->scl_io_num,
        .sda_pullup_en = bus_cfg->flags.enable_internal_pullup,
        .scl_pullup_en = bus_cfg->flags.enable_internal_pullup,
        .master.clk_speed = battery_config->clock_hz,
    };
    i2c_bus = i2c_bus_create(bus_cfg->i2c_port, &cfg);
    ESP_RETURN_ON_FALSE(i2c_bus, ESP_FAIL, TAG, "I2C bus wrapper");
    ESP_RETURN_ON_ERROR(create_battery(battery_capacity_mah), TAG, "battery driver");
    *handle = bq27220;
    return ESP_OK;
}
ESP_BOARD_ENTRY_IMPLEMENT(battery, battery_init, sensor_deinit);
esp_err_t board_bat_init(uint16_t mah)
{
    battery_capacity_mah = mah;
    void *handle;
    return board_device("battery", &handle);
}
