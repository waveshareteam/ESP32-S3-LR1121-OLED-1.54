/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __UI_H__
#define __UI_H__

#include "app_state.h"
#include "board_support.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif
 
LV_IMAGE_DECLARE(Battery4_png);    // png/Battery4.png
LV_IMAGE_DECLARE(Battery3_png);    // png/Battery3.png
LV_IMAGE_DECLARE(Battery2_png);    // png/Battery2.png
LV_IMAGE_DECLARE(Battery1_png);    // png/Battery1.png
LV_IMAGE_DECLARE(Charge_png);    // png/Charge.png
LV_IMAGE_DECLARE(Hz);    // png/Hz.png
LV_IMAGE_DECLARE(SF);    // png/SF.png
LV_IMAGE_DECLARE(BW);    // png/BW.png
LV_IMAGE_DECLARE(CR);    // png/CR.png
LV_IMAGE_DECLARE(US);    // png/US.png
LV_IMAGE_DECLARE(PW);    // png/PW.png
LV_IMAGE_DECLARE(ICON_BR);
LV_IMAGE_DECLARE(ICON_DV);
LV_IMAGE_DECLARE(ICON_RX);
LV_IMAGE_DECLARE(MD_G);
LV_IMAGE_DECLARE(MD_L);


void ui_init(void);
void lvgl_ptt_timer_cb(lv_timer_t *timer);
void ui_refresh_factory_test_status(void);
void ui_refresh_time_status(void);
void ui_refresh_battery_status(void);
void ui_refresh_status_bar(void);
void lvgl_rtc_bat_timer_cb(lv_timer_t *timer);
void key_scan_timer_cb(lv_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif
