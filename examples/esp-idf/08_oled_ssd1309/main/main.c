/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "board_support.h"
#include "esp_log.h"

static const char *TAG = "oled_ssd1309";

static void lvgl_oled_demo(lv_display_t *display)
{
    lv_obj_t *scr = lv_display_get_screen_active(display);
    lv_obj_set_style_text_font(scr, &lv_font_unscii_8, 0);

    lv_obj_t *label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "SSD1309 128x64");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *img = lv_image_create(scr);
    lv_obj_set_style_text_font(img, &lv_font_montserrat_16, 0);
    lv_image_set_src(img, LV_SYMBOL_WIFI);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 16, 0);

    lv_obj_t *img2 = lv_image_create(scr);
    lv_obj_set_style_text_font(img2, &lv_font_montserrat_16, 0);
    lv_image_set_src(img2, LV_SYMBOL_BATTERY_FULL);
    lv_obj_align(img2, LV_ALIGN_RIGHT_MID, -16, 0);

    lv_obj_t *label_info = lv_label_create(scr);
    lv_label_set_text(label_info, "1.54 inch OLED");
    lv_obj_align(label_info, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void app_main(void)
{
    lv_display_t *display = board_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to start display");
        return;
    }

    if (!board_display_lock(-1)) {
        ESP_LOGE(TAG, "Failed to lock display");
        return;
    }
    lvgl_oled_demo(display);
    board_display_unlock();
    ESP_LOGI(TAG, "OLED demo ready");
}
