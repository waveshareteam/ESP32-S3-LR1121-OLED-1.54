/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>

#include "peripheral_guard.h"
#include "radio_device.h"
#include "radio_settings.h"
#include "ui.h"

// UI objects are kept globally because LVGL timers refresh them after creation.
lv_obj_t *page1;
lv_obj_t *page2;

lv_obj_t * label_rtc = NULL;
lv_obj_t * label_bat = NULL;
lv_obj_t * label_factory_test = NULL;
lv_obj_t * bat_Image = NULL;
lv_obj_t * mode_btn = NULL;
lv_obj_t * label_mode = NULL;
lv_obj_t * label_freq = NULL;
lv_obj_t * label_unit = NULL;
lv_obj_t * label_params = NULL;
lv_obj_t * label_setting_title = NULL;

lv_group_t * g = NULL;
lv_obj_t * ptt_btn = NULL;
lv_obj_t * label_ptt = NULL;

lv_obj_t * target_roller = NULL;

lv_obj_t * md_btn = NULL;
lv_obj_t * md_Image = NULL;
lv_obj_t * md_Roller = NULL;
lv_obj_t * band_Roller = NULL;

lv_obj_t * hz_btn = NULL;
lv_obj_t * hz_Image = NULL;
lv_obj_t * hz_Roller = NULL;

lv_obj_t * sf_btn = NULL;
lv_obj_t * sf_Image = NULL;
lv_obj_t * sf_Roller = NULL;

lv_obj_t * bw_btn = NULL;
lv_obj_t * bw_Image = NULL;
lv_obj_t * bw_Roller = NULL;

lv_obj_t * cr_btn = NULL;
lv_obj_t * cr_Image = NULL;
lv_obj_t * cr_Roller = NULL;

lv_obj_t * pwr_btn = NULL;
lv_obj_t * pwr_Image = NULL;
lv_obj_t * pwr_Roller = NULL;

char buf[256];

uint8_t current_page = 0;  

// The monochrome panel mapping is inverted, so helpers express logical UI colors.
static lv_color_t ui_black(void)
{
    return lv_color_hex(0xFFFFFF);
}

static lv_color_t ui_white(void)
{
    return lv_color_hex(0x000000);
}

static void style_text(lv_obj_t *obj, const lv_font_t *font)
{
    lv_obj_set_style_text_color(obj, ui_white(), 0);
    lv_obj_set_style_text_font(obj, font, 0);
}

static void style_panel(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, ui_black(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, ui_white(), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void style_radio_button(lv_obj_t *btn)
{
    lv_obj_remove_style_all(btn);
    style_panel(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_outline_color(btn, ui_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn, 1, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn, 1, LV_STATE_FOCUSED);
    lv_obj_set_style_shadow_width(btn, 0, 0);
}

static void style_roller_selected(lv_obj_t *roller, lv_style_selector_t selector)
{
    lv_obj_set_style_bg_color(roller, ui_white(), selector);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, selector);
    lv_obj_set_style_bg_grad_dir(roller, LV_GRAD_DIR_NONE, selector);
    lv_obj_set_style_bg_image_opa(roller, LV_OPA_TRANSP, selector);
    lv_obj_set_style_text_color(roller, ui_black(), selector);
    lv_obj_set_style_text_opa(roller, LV_OPA_COVER, selector);
    lv_obj_set_style_text_font(roller, &lv_font_unscii_8, selector);
    lv_obj_set_style_border_width(roller, 0, selector);
    lv_obj_set_style_radius(roller, 0, selector);
    lv_obj_set_style_shadow_width(roller, 0, selector);
    lv_obj_set_style_outline_width(roller, 0, selector);
}

static lv_style_selector_t ui_part_state(lv_part_t part, lv_state_t state = static_cast<lv_state_t>(0))
{
    return static_cast<lv_style_selector_t>(part) | static_cast<lv_style_selector_t>(state);
}

static void style_roller(lv_obj_t *roller)
{
    lv_obj_remove_style_all(roller);
    style_panel(roller);
    lv_obj_set_style_text_color(roller, ui_white(), 0);
    lv_obj_set_style_text_font(roller, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(roller, 0, 0);
    lv_obj_set_style_anim_duration(roller, 200, 0);
    lv_obj_set_style_outline_width(roller, 0, 0);
    style_roller_selected(roller, ui_part_state(LV_PART_SELECTED));
    style_roller_selected(roller, ui_part_state(LV_PART_SELECTED, LV_STATE_FOCUSED));
    style_roller_selected(roller, ui_part_state(LV_PART_SELECTED, LV_STATE_EDITED));
    style_roller_selected(roller, ui_part_state(LV_PART_SELECTED, LV_STATE_FOCUS_KEY));
}

static const char *factory_result_text(factory_test_result_t result)
{
    switch (result) {
    case FACTORY_TEST_OK:
        return "O";
    case FACTORY_TEST_FAIL:
        return "F";
    case FACTORY_TEST_PENDING:
    default:
        return "-";
    }
}

void ui_refresh_factory_test_status(void)
{
    if (!label_factory_test) {
        return;
    }

    snprintf(buf, sizeof(buf), "SD:%s WF:%s",
             factory_result_text(sdcard_test_result),
             factory_result_text(wifi_test_result));
    lv_label_set_text(label_factory_test, buf);
}

static void ui_refresh_ptt_status(void)
{
    if (!ptt_btn || !label_ptt) return;

    if (Lora_Transmission_Mode) {
        lv_label_set_text(label_ptt, "TX");
        lv_obj_set_style_bg_color(ptt_btn, ui_white(), 0);
        lv_obj_set_style_bg_opa(ptt_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ptt_btn, ui_white(), 0);
        lv_obj_set_style_text_color(ptt_btn, ui_black(), 0);
        lv_obj_set_style_text_color(label_ptt, ui_black(), 0);
        lv_obj_set_style_text_opa(label_ptt, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(label_ptt, ui_white(), 0);
        lv_obj_set_style_bg_opa(label_ptt, LV_OPA_COVER, 0);
    } else {
        lv_label_set_text(label_ptt, "RX");
        lv_obj_set_style_bg_color(ptt_btn, ui_black(), 0);
        lv_obj_set_style_bg_opa(ptt_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ptt_btn, ui_white(), 0);
        lv_obj_set_style_text_color(ptt_btn, ui_white(), 0);
        lv_obj_set_style_text_color(label_ptt, ui_white(), 0);
        lv_obj_set_style_text_opa(label_ptt, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(label_ptt, ui_black(), 0);
        lv_obj_set_style_bg_opa(label_ptt, LV_OPA_TRANSP, 0);
    }

    lv_obj_center(label_ptt);
    lv_obj_invalidate(ptt_btn);
}

// Keep the mode, frequency, parameter row and tool icons in one refresh path.
static void ui_refresh_radio_status(void)
{
    int freq_mhz = (int)(radio_frequency_mhz + 0.5f);

    if (label_mode) {
        snprintf(buf, sizeof(buf), "%c-%s",
                 radio_modem == RADIO_MODEM_GFSK ? 'G' : 'L',
                 radio_band_label());
        lv_label_set_text(label_mode, buf);
    }

    if (label_freq) {
        snprintf(buf, sizeof(buf), "%03d.0", freq_mhz);
        lv_label_set_text(label_freq, buf);
    }

    if (label_unit) {
        lv_label_set_text(label_unit, "MHz");
    }

    if (label_params) {
        if (radio_modem == RADIO_MODEM_LORA) {
            snprintf(buf, sizeof(buf), "SF%u BW%s CR4/%u P%d",
                     radio_lora_spreading_factor,
                     radio_format_bandwidth(radio_lora_bandwidth_khz),
                     radio_lora_coding_rate,
                     (int)radio_tx_power_dbm);
        } else {
            char br[16];
            char dev[16];
            char rx[16];
            radio_format_decimal(br, sizeof(br), radio_gfsk_bit_rate_kbps);
            radio_format_decimal(dev, sizeof(dev), radio_gfsk_freq_dev_khz);
            radio_format_decimal(rx, sizeof(rx), radio_gfsk_rx_bw_khz);
            snprintf(buf, sizeof(buf), "BR%s DEV%s RX%s P%d",
                     br, dev, rx, (int)radio_tx_power_dbm);
        }
        lv_label_set_text(label_params, buf);
    }

    if (md_Image) {
        lv_image_set_src(md_Image, (radio_modem == RADIO_MODEM_GFSK) ? &MD_G : &MD_L);
    }
    if (sf_Image) {
        lv_image_set_src(sf_Image, (radio_modem == RADIO_MODEM_GFSK) ? &ICON_BR : &SF);
    }
    if (bw_Image) {
        lv_image_set_src(bw_Image, (radio_modem == RADIO_MODEM_GFSK) ? &ICON_DV : &BW);
    }
    if (cr_Image) {
        lv_image_set_src(cr_Image, (radio_modem == RADIO_MODEM_GFSK) ? &ICON_RX : &CR);
    }
}

// Called when page transition animation finishes
// Used to focus the active roller and enable editing mode
static void anim_ready_cb(lv_anim_t * a)
{
    if (target_roller) {
        lv_group_focus_obj(target_roller);

        // Enable editing so encoder/keys control roller directly
        lv_group_set_editing(g, true);
    }
}

// Generic Y-axis animation helper
static void anim_move_y(lv_obj_t *obj, int32_t from, int32_t to, uint16_t time, lv_anim_ready_cb_t cb)
{
    lv_anim_t a;
    lv_anim_init(&a);

    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);

    if (cb) {
        lv_anim_set_ready_cb(&a, cb);
    }

    lv_anim_start(&a);
}

// Switch between main page (0) and setting page (1)
void switch_page(uint8_t target_page)
{
    if (target_page == current_page) return;

    if (target_page == 1) {
        // Slide in page2 from bottom
        lv_obj_set_y(page2, 64);
        anim_move_y(page2, 64, 0, 200, anim_ready_cb);

        // Slide out page1 to top
        anim_move_y(page1, 0, -64, 200, NULL);
    }
    else {
        // Slide in page1 from top
        lv_obj_set_y(page1, -64);
        anim_move_y(page1, -64, 0, 200, NULL);

        // Slide out page2 to bottom
        anim_move_y(page2, 0, 64, 200, NULL);

        // Exit editing mode when returning to main UI
        lv_group_set_editing(g, false);
    }

    current_page = target_page;
}

// Button event handler
static void Button_event_cb(void *arg, void *data)
{
    (void)data;
    button_event_t event = iot_button_get_event((button_handle_t)arg);

    if (event == BUTTON_PRESS_DOWN || event == BUTTON_PRESS_UP) {
        Boot_Key_Flag = (gpio_get_level(PTT_KEY_GPIO) == 0);
    }
}

// Common exit logic after selecting a value from roller
static void roller_apply_and_exit(lv_obj_t *roller, lv_obj_t *btn)
{
    lv_obj_add_flag(roller, LV_OBJ_FLAG_HIDDEN);

    // Return focus to originating button
    lv_group_focus_obj(btn);

    // Return to main page
    switch_page(0);
}

// Open a roller and switch to setting page
static void open_roller(lv_obj_t *roller, const char *title)
{
    lv_obj_remove_flag(roller, LV_OBJ_FLAG_HIDDEN);

    // Save target for focus after animation
    target_roller = roller;

    if (label_setting_title) {
        lv_label_set_text(label_setting_title, title);
    }

    switch_page(1);
}

static void set_roller_options(lv_obj_t *roller, const char *options, uint8_t selected)
{
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
}

static void refresh_frequency_roller_options(void)
{
    if (!hz_Roller) {
        return;
    }

    set_roller_options(hz_Roller, radio_frequency_options(), radio_freq_to_index(last_freq_mhz));
}

// Frequency selection handler
static void md_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);
    uint8_t index = lv_roller_get_selected(roller);

    if (index != last_md) {
        radio_modem = (index == 0) ? RADIO_MODEM_GFSK : RADIO_MODEM_LORA;
        radio_reconfigure_requested = true;
        lv_image_set_src(md_Image, (radio_modem == RADIO_MODEM_GFSK) ? &MD_G : &MD_L);
        last_md = index;
        radio_settings_save();
    }

    ui_refresh_radio_status();
    roller_apply_and_exit(md_Roller, md_btn);
}

static void band_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);
    uint8_t index = lv_roller_get_selected(roller);
    radio_band_t next_band = (index == RADIO_BAND_LF) ? RADIO_BAND_LF : RADIO_BAND_HF;

    if (next_band != radio_band) {
        radio_select_band(next_band);
        radio_reconfigure_requested = true;
        radio_settings_save();
        refresh_frequency_roller_options();
    }

    ui_refresh_radio_status();
    roller_apply_and_exit(band_Roller, mode_btn);
}

// Frequency selection handler
static void hz_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);
    float freq_mhz = radio_index_to_freq(lv_roller_get_selected(roller));

    // Only apply if value changed
    if (freq_mhz != last_freq_mhz) {
        radio_set_frequency_mhz(freq_mhz);
        radio_reconfigure_requested = true;
        radio_settings_save();
    }

    ui_refresh_radio_status();
    roller_apply_and_exit(hz_Roller, hz_btn);
}

// Spreading factor selection handler
// The same three parameter rollers map to LoRa SF/BW/CR or GFSK BR/DEV/RXBW.
static void sf_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);
    uint8_t index = lv_roller_get_selected(roller);

    if (radio_modem == RADIO_MODEM_LORA) {
        if (index >= SF_OPTION_COUNT) index = last_sf;
        if (index != last_sf) {
            lora_config.spreading_factor = sf_values[index];
            radio_lora_spreading_factor = lora_config.spreading_factor;
            peripheral_lock();
            radio.setSpreadingFactor(lora_config.spreading_factor);
            peripheral_unlock();
            last_sf = index;
            radio_settings_save();
        }
    } else {
        if (index >= GFSK_BR_OPTION_COUNT) index = last_gfsk_br;
        if (index != last_gfsk_br) {
            radio_gfsk_bit_rate_kbps = gfsk_br_values[index];
            radio_reconfigure_requested = true;
            last_gfsk_br = index;
            radio_settings_save();
        }
    }

    ui_refresh_radio_status();
    roller_apply_and_exit(sf_Roller, sf_btn);
}

static void bw_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);

    // Get current selected index
    uint8_t index = lv_roller_get_selected(roller);
    if (radio_modem == RADIO_MODEM_LORA) {
        if (index >= BW_OPTION_COUNT) index = last_bw;
        if (index != last_bw) {
            lora_config.bandwidth = bw_values[index];
            radio_lora_bandwidth_khz = lora_config.bandwidth;
            peripheral_lock();
            radio.setBandwidth(lora_config.bandwidth);
            peripheral_unlock();
            last_bw = index;
            radio_settings_save();
        }
    } else {
        if (index >= GFSK_DEV_OPTION_COUNT) index = last_gfsk_dev;
        if (index != last_gfsk_dev) {
            radio_gfsk_freq_dev_khz = gfsk_dev_values[index];
            radio_reconfigure_requested = true;
            last_gfsk_dev = index;
            radio_settings_save();
        }
    }
    
    ui_refresh_radio_status();
    roller_apply_and_exit(bw_Roller, bw_btn);
}

static void cr_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);

    // Get current selected index
    uint8_t index = lv_roller_get_selected(roller);
    if (radio_modem == RADIO_MODEM_LORA) {
        if (index >= CR_OPTION_COUNT) index = last_cr;
        if (index != last_cr) {
            lora_config.coding_rate = index + 4;
            radio_lora_coding_rate = lora_config.coding_rate;
            peripheral_lock();
            radio.setCodingRate(lora_config.coding_rate);
            peripheral_unlock();
            last_cr = index;
            radio_settings_save();
        }
    } else {
        if (index >= GFSK_RX_OPTION_COUNT) index = last_gfsk_rx;
        if (index != last_gfsk_rx) {
            radio_gfsk_rx_bw_khz = gfsk_rx_bw_values[index];
            radio_reconfigure_requested = true;
            last_gfsk_rx = index;
            radio_settings_save();
        }
    }
    
    ui_refresh_radio_status();
    roller_apply_and_exit(cr_Roller, cr_btn);
}

static void pwr_roller_event_cb(lv_event_t * e)
{
    lv_obj_t * roller = (lv_obj_t *)lv_event_get_target(e);

    // Get current selected index
    uint8_t index = lv_roller_get_selected(roller);
    if (index != last_pwr) {
        lora_config.tx_power_dbm = tx_power_values[index];
        radio_tx_power_dbm = lora_config.tx_power_dbm;
        peripheral_lock();
        radio.setOutputPower(lora_config.tx_power_dbm, true, lora_config.pa_ramp_time);
        peripheral_unlock();
        last_pwr = index;
        radio_settings_save();
    };
    
    ui_refresh_radio_status();
    roller_apply_and_exit(pwr_Roller, pwr_btn);
}

// Open modem selector
static void md_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(md_Roller, "MODE");
    }
}

static void band_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        set_roller_options(band_Roller, RADIO_BAND_OPTIONS, last_band);
        open_roller(band_Roller, "BAND");
    }
}

// Open frequency selector
static void hz_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        refresh_frequency_roller_options();
        open_roller(hz_Roller, "FREQ");
    }
}

// Open spreading factor selector
static void sf_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (radio_modem == RADIO_MODEM_LORA) {
            set_roller_options(sf_Roller, LORA_SF_OPTIONS, last_sf);
            open_roller(sf_Roller, "SPREAD");
        } else {
            set_roller_options(sf_Roller, GFSK_BR_OPTIONS, last_gfsk_br);
            open_roller(sf_Roller, "BITRATE");
        }
    }
}

static void bw_btn_event_cb(lv_event_t * e)
{

    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (radio_modem == RADIO_MODEM_LORA) {
            set_roller_options(bw_Roller, LORA_BW_OPTIONS, last_bw);
            open_roller(bw_Roller, "BAND");
        } else {
            set_roller_options(bw_Roller, GFSK_DEV_OPTIONS, last_gfsk_dev);
            open_roller(bw_Roller, "DEVIAT");
        }
    }
}

static void cr_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (radio_modem == RADIO_MODEM_LORA) {
            set_roller_options(cr_Roller, LORA_CR_OPTIONS, last_cr);
            open_roller(cr_Roller, "CODING");
        } else {
            set_roller_options(cr_Roller, GFSK_RX_OPTIONS, last_gfsk_rx);
            open_roller(cr_Roller, "RXBW");
        }
    }
}

static void pwr_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        open_roller(pwr_Roller, "PWR");
    }
}

// Build a compact two-page 128x64 radio UI: dashboard plus roller settings.
void ui_create_pages(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, ui_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, &lv_font_unscii_8, 0);

    /* ===== Fixed status bar ===== */
    label_rtc = lv_label_create(scr);
    lv_obj_remove_style_all(label_rtc);
    lv_obj_set_pos(label_rtc, 2, 0);
    lv_obj_set_size(label_rtc, 42, 10);
    style_text(label_rtc, &lv_font_unscii_8);
    lv_label_set_long_mode(label_rtc, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(label_rtc,"--:--");

    label_bat = lv_label_create(scr);
    lv_obj_remove_style_all(label_bat);
    lv_obj_set_pos(label_bat, 82, 0);
    lv_obj_set_size(label_bat, 32, 10);
    style_text(label_bat, &lv_font_unscii_8);
    lv_obj_set_style_text_align(label_bat, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(label_bat, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(label_bat,"--%");

    bat_Image = lv_image_create(scr);
    lv_obj_set_pos(bat_Image, 116, 0);
    lv_image_set_src(bat_Image, &Battery4_png);

    label_factory_test = lv_label_create(scr);
    lv_obj_remove_style_all(label_factory_test);
    lv_obj_set_pos(label_factory_test, 50, 0);
    lv_obj_set_size(label_factory_test, 29, 10);
    style_text(label_factory_test, &lv_font_unscii_8);
    lv_obj_set_style_text_align(label_factory_test, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(label_factory_test, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    ui_refresh_factory_test_status();

    lv_obj_t *ui_layer = lv_obj_create(scr);
    lv_obj_remove_style_all(ui_layer);
    lv_obj_set_size(ui_layer, 128, 64);
    lv_obj_align(ui_layer, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_foreground(label_factory_test);
    lv_obj_move_foreground(label_rtc);
    lv_obj_move_foreground(label_bat);
    lv_obj_move_foreground(bat_Image);

    /* ===== Page1 ===== */
    page1 = lv_obj_create(ui_layer);
    lv_obj_remove_style_all(page1);   // Remove default style
    lv_obj_set_size(page1, 128, 64);
    lv_obj_align(page1, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Create input device */
    lv_indev_t * indev = board_display_get_input_dev();

    /* Create group */
    g = lv_group_create();

    /* Bind group */
    lv_indev_set_group(indev, g);

    /* ===== Radio dashboard ===== */
    ptt_btn = lv_btn_create(page1);
    style_radio_button(ptt_btn);
    lv_obj_set_size(ptt_btn, 27, 17);
    lv_obj_set_pos(ptt_btn, 2, 14);
    lv_obj_remove_flag(ptt_btn, LV_OBJ_FLAG_CLICKABLE);

    label_ptt = lv_label_create(ptt_btn);
    lv_obj_remove_style_all(label_ptt);
    lv_obj_center(label_ptt);
    style_text(label_ptt, &lv_font_unscii_8);
    lv_label_set_text(label_ptt,"RX");

    label_freq = lv_label_create(page1);
    lv_obj_set_pos(label_freq, 35, 11);
    style_text(label_freq, &lv_font_montserrat_20);
    lv_label_set_text(label_freq, "868.0");

    label_unit = lv_label_create(page1);
    lv_obj_set_pos(label_unit, 101, 24);
    style_text(label_unit, &lv_font_unscii_8);
    lv_label_set_text(label_unit, "MHz");

    mode_btn = lv_btn_create(page1);
    lv_obj_remove_style_all(mode_btn);
    lv_obj_set_pos(mode_btn, 96, 13);
    lv_obj_set_size(mode_btn, 30, 10);
    lv_obj_add_flag(mode_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(mode_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mode_btn, 0, 0);
    lv_obj_set_style_outline_color(mode_btn, ui_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(mode_btn, 1, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(mode_btn, 1, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(mode_btn, band_btn_event_cb, LV_EVENT_CLICKED, NULL);

    label_mode = lv_label_create(mode_btn);
    lv_obj_remove_style_all(label_mode);
    lv_obj_set_size(label_mode, 30, 10);
    lv_obj_set_pos(label_mode, 0, 0);
    style_text(label_mode, &lv_font_unscii_8);
    lv_label_set_long_mode(label_mode, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(label_mode, "G-HF");

    label_params = lv_label_create(page1);
    lv_obj_set_size(label_params, 124, 10);
    lv_obj_set_pos(label_params, 2, 34);
    style_text(label_params, &lv_font_unscii_8);
    lv_label_set_long_mode(label_params, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_label_set_text(label_params, "BR50 DEV25 RX117 P22");
    
    button_handle_t btn = board_button("button_ptt");
    ESP_ERROR_CHECK(btn ? ESP_OK : ESP_FAIL);

    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, Button_event_cb, NULL);
    iot_button_register_cb(btn, BUTTON_PRESS_UP, NULL, Button_event_cb, NULL);

    //MD
    md_btn = lv_btn_create(page1);
    style_radio_button(md_btn);
    lv_obj_set_size(md_btn, 19, 16);
    lv_obj_set_pos(md_btn, 2, 46);

    md_Image = lv_image_create(md_btn);
    lv_obj_center(md_Image);
    lv_image_set_src(md_Image, &MD_G);

    lv_obj_add_event_cb(md_btn, md_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //Hz
    hz_btn = lv_btn_create(page1);
    style_radio_button(hz_btn);
    lv_obj_set_size(hz_btn, 19, 16);
    lv_obj_set_pos(hz_btn, 23, 46);

    hz_Image = lv_image_create(hz_btn);
    lv_obj_center(hz_Image);
    lv_image_set_src(hz_Image, &Hz);

    lv_obj_add_event_cb(hz_btn, hz_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    //SF
    sf_btn = lv_btn_create(page1);
    style_radio_button(sf_btn);
    lv_obj_set_size(sf_btn, 19, 16);
    lv_obj_set_pos(sf_btn, 44, 46);

    sf_Image = lv_image_create(sf_btn);
    lv_obj_center(sf_Image);
    lv_image_set_src(sf_Image, &ICON_BR);

    lv_obj_add_event_cb(sf_btn, sf_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //BW
    bw_btn = lv_btn_create(page1);
    style_radio_button(bw_btn);
    lv_obj_set_size(bw_btn, 19, 16);
    lv_obj_set_pos(bw_btn, 65, 46);

    bw_Image = lv_image_create(bw_btn);
    lv_obj_center(bw_Image);
    lv_image_set_src(bw_Image, &ICON_DV);

    lv_obj_add_event_cb(bw_btn, bw_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //CR
    cr_btn = lv_btn_create(page1);
    style_radio_button(cr_btn);
    lv_obj_set_size(cr_btn, 19, 16);
    lv_obj_set_pos(cr_btn, 86, 46);

    cr_Image = lv_image_create(cr_btn);
    lv_obj_center(cr_Image);
    lv_image_set_src(cr_Image, &ICON_RX);

    lv_obj_add_event_cb(cr_btn, cr_btn_event_cb, LV_EVENT_CLICKED, NULL);

    //PWR
    pwr_btn = lv_btn_create(page1);
    style_radio_button(pwr_btn);
    lv_obj_set_size(pwr_btn, 19, 16);
    lv_obj_set_pos(pwr_btn, 107, 46);

    pwr_Image = lv_image_create(pwr_btn);
    lv_obj_center(pwr_Image);
    lv_image_set_src(pwr_Image, &PW);

    lv_obj_add_event_cb(pwr_btn, pwr_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // lv_group_add_obj(g, ptt_btn);
    lv_group_add_obj(g, md_btn);
    lv_group_add_obj(g, hz_btn);
    lv_group_add_obj(g, sf_btn);
    lv_group_add_obj(g, bw_btn);
    lv_group_add_obj(g, cr_btn);
    lv_group_add_obj(g, pwr_btn);
    lv_group_add_obj(g, mode_btn);
    
    /* Default focus */
    lv_group_focus_obj(md_btn);

    /******************************************* ===== Page2 ===== ***********************************************/
    page2 = lv_obj_create(ui_layer);
    lv_obj_remove_style_all(page2);
    lv_obj_set_size(page2, 128, 64);
    lv_obj_set_style_bg_color(page2, ui_black(), 0);
    lv_obj_set_style_bg_opa(page2, LV_OPA_COVER, 0);

    /* Initially placed at the bottom of the screen (hidden) */
    lv_obj_align(page2, LV_ALIGN_TOP_LEFT, 0, 64);

    label_setting_title = lv_label_create(page2);
    lv_obj_set_pos(label_setting_title, 2, 11);
    style_text(label_setting_title, &lv_font_unscii_8);
    lv_label_set_text(label_setting_title, "MODE");

    //MD
    md_Roller = lv_roller_create(page2);
    style_roller(md_Roller);
    lv_obj_set_size(md_Roller, 94, 36);
    lv_obj_set_pos(md_Roller, 17, 26);
    lv_roller_set_options(md_Roller,
                          "GFSK\nLoRa",
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(md_Roller, last_md, LV_ANIM_OFF);
    lv_obj_add_event_cb(md_Roller, md_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(md_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, md_Roller);

    //BAND
    band_Roller = lv_roller_create(page2);
    style_roller(band_Roller);
    lv_obj_set_size(band_Roller, 94, 36);
    lv_obj_set_pos(band_Roller, 17, 26);
    lv_roller_set_options(band_Roller,
                          RADIO_BAND_OPTIONS,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(band_Roller, last_band, LV_ANIM_OFF);
    lv_obj_add_event_cb(band_Roller, band_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(band_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, band_Roller);
    
    //Hz
    hz_Roller = lv_roller_create(page2);
    style_roller(hz_Roller);
    lv_obj_set_size(hz_Roller, 94, 36);
    lv_obj_set_pos(hz_Roller, 17, 26);
    lv_roller_set_options(hz_Roller,
                          radio_frequency_options(),
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(hz_Roller, radio_freq_to_index(last_freq_mhz), LV_ANIM_OFF);
    lv_obj_add_event_cb(hz_Roller, hz_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(hz_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, hz_Roller);  
    
    //SF
    sf_Roller = lv_roller_create(page2);
    style_roller(sf_Roller);
    lv_obj_set_size(sf_Roller, 94, 36);
    lv_obj_set_pos(sf_Roller, 17, 26);
    // Roller text display
    lv_roller_set_options(sf_Roller,
                          GFSK_BR_OPTIONS,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(sf_Roller, last_gfsk_br, LV_ANIM_OFF);
    lv_obj_add_event_cb(sf_Roller, sf_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(sf_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, sf_Roller);

    //BW
    bw_Roller = lv_roller_create(page2);
    style_roller(bw_Roller);
    lv_obj_set_size(bw_Roller, 94, 36);
    lv_obj_set_pos(bw_Roller, 17, 26);
    // Roller text display
    lv_roller_set_options(bw_Roller,
                          GFSK_DEV_OPTIONS,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(bw_Roller, last_gfsk_dev, LV_ANIM_OFF);
    lv_obj_add_event_cb(bw_Roller, bw_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(bw_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, bw_Roller);
    
    //CR
    cr_Roller = lv_roller_create(page2);
    style_roller(cr_Roller);
    lv_obj_set_size(cr_Roller, 94, 36);
    lv_obj_set_pos(cr_Roller, 17, 26);
    // Roller text display
    lv_roller_set_options(cr_Roller,
                          GFSK_RX_OPTIONS,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(cr_Roller, last_gfsk_rx, LV_ANIM_OFF);
    lv_obj_add_event_cb(cr_Roller, cr_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(cr_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, cr_Roller);

    //PWR
    pwr_Roller = lv_roller_create(page2);
    style_roller(pwr_Roller);
    lv_obj_set_size(pwr_Roller, 94, 36);
    lv_obj_set_pos(pwr_Roller, 17, 26);
    // Roller text display
    lv_roller_set_options(pwr_Roller,
                          TX_POWER_OPTIONS,
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(pwr_Roller, last_pwr, LV_ANIM_OFF);
    lv_obj_add_event_cb(pwr_Roller, pwr_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(pwr_Roller, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(g, pwr_Roller);

    ui_refresh_radio_status();
    ui_refresh_ptt_status();

}

void ui_init(void)
{
    ui_create_pages();
}

// Update PTT state label periodically
void lvgl_ptt_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_refresh_ptt_status();
}

void ui_refresh_time_status(void)
{
    if (rtc_test_result == FACTORY_TEST_OK) {
        snprintf(buf, sizeof(buf),
                 "%02d:%02d",
                 time_info.hour,
                 time_info.min);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }

    if (label_rtc) {
        lv_label_set_text(label_rtc, buf);
    }
}

void ui_refresh_battery_status(void)
{
    if (battery_test_result != FACTORY_TEST_OK) {
        if (label_bat) {
            lv_label_set_text(label_bat, "--%");
        }
        if (bat_Image) {
            lv_image_set_src(bat_Image, &Battery1_png);
        }
        return;
    }

    uint16_t soc = bat_info.soc > 100 ? 100 : bat_info.soc;
    snprintf(buf, sizeof(buf), "%u%%", soc);
    if (label_bat) {
        lv_label_set_text(label_bat, buf);
    }

    if (!bat_Image) {
        return;
    }

    // Update battery icon based on charge state
    if (bat_info.charging) {
        lv_image_set_src(bat_Image, &Charge_png); // charging
    } else {
        if (soc > 75) {
            lv_image_set_src(bat_Image, &Battery4_png);
        } else if (soc > 50) {
            lv_image_set_src(bat_Image, &Battery3_png);
        } else if (soc > 25) {
            lv_image_set_src(bat_Image, &Battery2_png);
        } else {
            lv_image_set_src(bat_Image, &Battery1_png);
        }
    }
}

void ui_refresh_status_bar(void)
{
    ui_refresh_factory_test_status();
    ui_refresh_time_status();
    ui_refresh_battery_status();
}

// Update RTC time and battery status
void lvgl_rtc_bat_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_refresh_status_bar();
}
