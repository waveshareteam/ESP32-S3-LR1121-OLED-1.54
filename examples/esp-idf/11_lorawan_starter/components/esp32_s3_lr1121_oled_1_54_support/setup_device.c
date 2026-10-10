// SPDX-License-Identifier: Apache-2.0
#include <string.h>
#include "board_support.h"
#include "esp_check.h"
#include "esp_board_periph.h"
#include "esp_board_entry.h"
#include "dev_button.h"
#include "dev_fs_fat.h"
#include "periph_spi.h"
#include "periph_gpio.h"
#include "gen_board_device_custom.h"
static const char *TAG = "ws_board";
// These startup helpers are called before application worker tasks start.
static const char *initialized_peripherals[32];
static size_t peripheral_count;
static const char *initialized_devices[16];
static size_t device_count;
sdmmc_card_t *bsp_sdcard;

esp_err_t board_peripheral(const char *name, void **handle)
{
    for (size_t i = 0; i < peripheral_count; ++i) {
        if (!strcmp(name, initialized_peripherals[i]))
            return esp_board_manager_get_periph_handle(name, handle);
    }
    ESP_RETURN_ON_FALSE(peripheral_count < 32, ESP_ERR_NO_MEM, TAG, "peripheral cache full");
    ESP_RETURN_ON_ERROR(esp_board_periph_init(name), TAG, "initialize %s", name);
    initialized_peripherals[peripheral_count++] = name;
    return esp_board_manager_get_periph_handle(name, handle);
}
esp_err_t board_device(const char *name, void **handle)
{
    for (size_t i = 0; i < device_count; ++i) {
        if (!strcmp(name, initialized_devices[i]))
            return esp_board_manager_get_device_handle(name, handle);
    }
    ESP_RETURN_ON_FALSE(device_count < 16, ESP_ERR_NO_MEM, TAG, "device cache full");
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(name), TAG, "initialize %s", name);
    initialized_devices[device_count++] = name;
    return esp_board_manager_get_device_handle(name, handle);
}
esp_err_t board_init(void)
{
    void *handle;
    return board_peripheral("i2c_main", &handle);
}
esp_err_t board_i2c_init(void) { return board_init(); }
i2c_master_bus_handle_t board_i2c_get_handle(void)
{
    void *bus = NULL;
    if (board_peripheral("i2c_main", &bus) != ESP_OK) return NULL;
    return bus;
}
button_handle_t board_button(const char *name)
{
    dev_button_handles_t *buttons = NULL;
    if (board_device(name, (void **)&buttons) != ESP_OK || !buttons->num_buttons) return NULL;
    return buttons->button_handles[0];
}
button_handle_t board_button_for_gpio(gpio_num_t gpio)
{
    switch (gpio) {
    case BOOT_KEY_GPIO: return board_button("button_boot");
    case PWR_KEY_GPIO: return board_button("button_power");
    case MENU_KEY_GPIO: return board_button("button_menu");
    case PTT_KEY_GPIO: return board_button("button_ptt");
    case WS_RTC_IRQ_GPIO: return board_button("button_rtc_irq");
    default: return NULL;
    }
}
int board_button_active_level(gpio_num_t gpio)
{
    return gpio == PWR_KEY_GPIO ? WS_POWER_ACTIVE_LEVEL : BUTTON_ACTIVE_LEVEL;
}
static int radio_init(void *config, int size, void **handle)
{
    if (size != sizeof(dev_custom_radio_config_t)) return ESP_ERR_INVALID_ARG;
    const dev_custom_radio_config_t *cfg = config;
    periph_spi_handle_t *bus = NULL;
    ESP_RETURN_ON_ERROR(board_peripheral("spi_radio", (void **)&bus), TAG, "radio bus");
    const char *pins[] = {"gpio_radio_cs", "gpio_radio_irq", "gpio_radio_reset", "gpio_radio_busy"};
    for (size_t i = 0; i < sizeof(pins)/sizeof(pins[0]); ++i) {
        void *gpio;
        ESP_RETURN_ON_ERROR(board_peripheral(pins[i], &gpio), TAG, "radio pin");
    }
    // RadioLib manually holds CS across complete LR1121 commands.
    spi_device_interface_config_t dev = {
        .clock_speed_hz = cfg->clock_hz, .mode = cfg->mode,
        .spics_io_num = -1, .queue_size = cfg->queue_size,
    };
    return spi_bus_add_device(bus->spi_port, &dev, (spi_device_handle_t *)handle);
}
static int radio_deinit(void *handle) { return spi_bus_remove_device(handle); }
ESP_BOARD_ENTRY_IMPLEMENT(radio, radio_init, radio_deinit);
esp_err_t board_spi_init(void)
{
    void *handle;
    return board_device("radio", &handle);
}
spi_device_handle_t board_spi_get_handle(void)
{
    void *handle = NULL;
    if (board_device("radio", &handle) != ESP_OK) return NULL;
    return handle;
}
esp_err_t board_sdcard_mount(void)
{
    dev_fs_fat_handle_t *sd = NULL;
    ESP_RETURN_ON_ERROR(board_device("sdcard", (void **)&sd), TAG, "SD card");
    bsp_sdcard = sd->card;
    return ESP_OK;
}
esp_err_t board_sdcard_unmount(void)
{
    ESP_RETURN_ON_ERROR(esp_board_manager_deinit_device_by_name("sdcard"), TAG, "SD unmount");
    for (size_t i = 0; i < device_count; ++i) {
        if (!strcmp(initialized_devices[i], "sdcard")) {
            initialized_devices[i] = initialized_devices[--device_count];
            break;
        }
    }
    bsp_sdcard = NULL;
    return ESP_OK;
}
esp_err_t board_audio_poweramp_enable(bool enable)
{
    periph_gpio_handle_t *amp = NULL;
    ESP_RETURN_ON_ERROR(board_peripheral("gpio_amplifier", (void **)&amp), TAG, "amplifier");
    return gpio_set_level(amp->gpio_num, enable);
}
