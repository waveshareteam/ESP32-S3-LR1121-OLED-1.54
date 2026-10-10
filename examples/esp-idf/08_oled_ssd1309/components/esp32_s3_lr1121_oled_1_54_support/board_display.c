// SPDX-License-Identifier: Apache-2.0
// Board Manager has no native I2C display_lcd subtype in 0.7.3~1.
// Keep only SSD1309 construction here; Adapter owns all LVGL runtime/buffers.
#include "sdkconfig.h"
#if CONFIG_WS_BOARD_DISPLAY
#include "board_support.h"
#include "esp_check.h"
#include "esp_board_entry.h"
#include "gen_board_device_custom.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_oled_ssd1309.h"
static const char *TAG = "ws_oled";
typedef struct { esp_lcd_panel_handle_t panel; esp_lcd_panel_io_handle_t io; } ws_oled_t;
static ws_oled_t oled;
static lv_display_t *display;
static lv_indev_t *navigation;
static int oled_init(void *config, int size, void **handle)
{
    if (size != sizeof(dev_custom_oled_config_t)) return ESP_ERR_INVALID_ARG;
    const dev_custom_oled_config_t *cfg = config;
    i2c_master_bus_handle_t bus = board_i2c_get_handle();
    if (!bus) return ESP_FAIL;
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = cfg->address, .scl_speed_hz = cfg->clock_hz,
        .control_phase_bytes = cfg->control_phase_bytes,
        .dc_bit_offset = cfg->dc_bit_offset,
        .lcd_cmd_bits = cfg->cmd_bits, .lcd_param_bits = cfg->param_bits,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &oled.io), TAG, "OLED I2C IO");
    esp_lcd_panel_ssd1309_config_t vendor = { .height = cfg->height };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = cfg->reset_gpio, .bits_per_pixel = cfg->bits_per_pixel,
        .vendor_config = &vendor,
    };
    esp_err_t err = esp_lcd_new_panel_ssd1309(oled.io, &panel_cfg, &oled.panel);
    if (err != ESP_OK) goto failed;
    err = esp_lcd_panel_init(oled.panel);
    if (err != ESP_OK) goto failed;
    err = esp_lcd_panel_invert_color(oled.panel, cfg->invert_color);
    if (err != ESP_OK) goto failed;
    err = esp_lcd_panel_disp_on_off(oled.panel, true);
    if (err != ESP_OK) goto failed;
    *handle = &oled;
    return ESP_OK;
failed:
    if (oled.panel) esp_lcd_panel_del(oled.panel);
    if (oled.io) esp_lcd_panel_io_del(oled.io);
    oled = (ws_oled_t){0};
    return err;
}
static int oled_deinit(void *handle)
{
    ws_oled_t *dev = handle;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_del(dev->panel), TAG, "panel delete");
    return esp_lcd_panel_io_del(dev->io);
}
ESP_BOARD_ENTRY_IMPLEMENT(oled, oled_init, oled_deinit);
lv_display_t *board_display_start(void)
{
    if (display) return display;
    ws_oled_t *dev = NULL;
    dev_custom_oled_config_t *panel_cfg = NULL;
    if (board_device("oled", (void **)&dev) != ESP_OK ||
        esp_board_manager_get_device_config("oled", (void **)&panel_cfg) != ESP_OK) return NULL;
    if (panel_cfg->num_fbs != 1) return NULL;
    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_cfg.task_stack_size = 8 * 1024;
    if (esp_lv_adapter_init(&adapter_cfg) != ESP_OK) return NULL;
    esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_MONO_DEFAULT_CONFIG(
        dev->panel, dev->io, panel_cfg->width, panel_cfg->height,
        (esp_lv_adapter_rotation_t)panel_cfg->rotation, ESP_LV_ADAPTER_MONO_LAYOUT_VTILED);
    // The SPI_MONO profile is also the official adapter path for I2C page OLEDs.
    lv_display_t *candidate = esp_lv_adapter_register_display(&disp_cfg);
    if (!candidate) goto failed;
    button_handle_t prev = board_button("button_boot");
    button_handle_t next = board_button("button_power");
    button_handle_t enter = board_button("button_menu");
    if (!prev || !next || !enter) goto failed;
    esp_lv_adapter_nav_buttons_config_t nav_cfg = {
        .disp = candidate, .button_prev = prev, .button_next = next, .button_enter = enter,
    };
    navigation = esp_lv_adapter_register_navigation_buttons(&nav_cfg);
    if (!navigation) goto failed;
    // Adapter 0.7.0 updates button states but never reads its event-mode indev.
    // Poll it from the adapter-owned LVGL task so navigation and release events
    // reach the UI, including encoder long-press/repeat handling.
    if (esp_lv_adapter_lock(-1) != ESP_OK) goto failed;
    lv_indev_set_mode(navigation, LV_INDEV_MODE_TIMER);
    esp_lv_adapter_unlock();
    if (esp_lv_adapter_start() != ESP_OK) goto failed;
    display = candidate;
    return display;
failed:
    esp_lv_adapter_deinit();
    navigation = NULL;
    return NULL;
}
lv_indev_t *board_display_get_input_dev(void) { return navigation; }
bool board_display_lock(int32_t timeout_ms) { return esp_lv_adapter_lock(timeout_ms) == ESP_OK; }
void board_display_unlock(void) { esp_lv_adapter_unlock(); }
// Compatibility names: an OLED has no backlight; these switch panel output.
esp_err_t board_display_backlight_on(void) { return esp_lcd_panel_disp_on_off(oled.panel, true); }
esp_err_t board_display_backlight_off(void) { return esp_lcd_panel_disp_on_off(oled.panel, false); }

#endif // CONFIG_WS_BOARD_DISPLAY
