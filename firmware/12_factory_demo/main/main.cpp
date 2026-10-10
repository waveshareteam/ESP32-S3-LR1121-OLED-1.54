/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "iot_button.h"
#include "nvs_flash.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "lvgl.h"
#include "app_state.h"
#include "audio_pipeline.h"
#include "peripheral_guard.h"
#include "radio_device.h"
#include "radio_runtime.h"
#include "radio_settings.h"
#include "ui.h"

static const char *TAG = "app_main";
static const uint32_t LOW_POWER_IDLE_TIMEOUT_MS = 30000;
static const gpio_num_t WAKE_BUTTON_GPIOS[] = {
    PTT_KEY_GPIO,
    MENU_KEY_GPIO,
    PWR_KEY_GPIO,
    BOOT_KEY_GPIO,
};
static const gpio_num_t LORA_WAKE_GPIO = BSP_SPI_INT;

static void halt_task()
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void halt_listen_error(int state)
{
    printf("[LR1121] Listen failed, code: %d\n", state);
    while (true) {
        hal->delay(10);
    }
}

// Shared radio packet buffer. Keep it packet-sized to avoid partial audio frames on air.
uint8_t Send_Receive_Package[LORA_TRANSMISSION_DATA_SIZE];
static bool lastState = false;
static bool radio_tx_busy = false;
static bool rtc_ready = false;
static bool battery_gauge_ready = false;

static factory_test_result_t factory_result_from_bool(bool ok)
{
    return ok ? FACTORY_TEST_OK : FACTORY_TEST_FAIL;
}

static bool is_valid_rtc_time(const pcf85063a_datetime_t &time)
{
    return time.year >= 1970 && time.year <= 2069 &&
           time.month >= 1 && time.month <= 12 &&
           time.day >= 1 && time.day <= 31 &&
           time.hour < 24 && time.min < 60 && time.sec < 60;
}

static bool is_valid_battery_info(const bsp_bat_info_t &info)
{
    return info.mv >= 2500 && info.mv <= 5000 && info.soc <= 100;
}

static bool update_battery_info()
{
    if (!battery_gauge_ready) {
        battery_test_result = FACTORY_TEST_FAIL;
        return false;
    }

    bsp_bat_info_t sampled_info = {};
    esp_err_t bat_ret = board_get_bat_info(&sampled_info);
    bool battery_ok = bat_ret == ESP_OK && is_valid_battery_info(sampled_info);
    battery_test_result = factory_result_from_bool(battery_ok);
    if (battery_ok) {
        bat_info = sampled_info;
    }

    return battery_ok;
}

// Read RTC into a temporary value so failed I2C reads cannot corrupt UI time.
static bool read_rtc_time()
{
    if (!rtc_ready) {
        return false;
    }

    pcf85063a_datetime_t new_time = {};
    esp_err_t ret = board_get_rtc_time_date(&new_time);
    if (ret != ESP_OK || !is_valid_rtc_time(new_time)) {
        return false;
    }

    time_info = new_time;
    return true;
}

static bool set_rtc_time_to_noon()
{
    if (!rtc_ready) {
        return false;
    }

    pcf85063a_datetime_t noon_time = {};
    esp_err_t ret = board_get_rtc_time_date(&noon_time);
    if (ret != ESP_OK || !is_valid_rtc_time(noon_time)) {
        noon_time.year = 2026;
        noon_time.month = 1;
        noon_time.day = 1;
        noon_time.dotw = 4;
    }

    noon_time.hour = 12;
    noon_time.min = 0;
    noon_time.sec = 0;

    ret = board_set_rtc_time_date(noon_time);
    if (ret != ESP_OK) {
        return false;
    }

    return read_rtc_time();
}

// LVGL calls must hold the display lock because the adapter task also runs LVGL.
static void refresh_time_status_now()
{
    board_display_lock(-1);
    ui_refresh_time_status();
    board_display_unlock();
}

static void refresh_battery_status_now()
{
    board_display_lock(-1);
    ui_refresh_battery_status();
    board_display_unlock();
}

static void refresh_factory_test_status_now()
{
    board_display_lock(-1);
    ui_refresh_factory_test_status();
    board_display_unlock();
}

static void start_status_timer()
{
    board_display_lock(-1);
    lv_timer_create(lvgl_rtc_bat_timer_cb, 300, NULL);
    board_display_unlock();
}

static void request_ptt_status_refresh()
{
    if (!board_display_lock(-1)) {
        return;
    }

    lv_timer_t *timer = lv_timer_create(lvgl_ptt_timer_cb, 1, NULL);
    if (timer) {
        lv_timer_set_repeat_count(timer, 1);
    }
    board_display_unlock();
}


static bool is_button_pressed(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == board_button_active_level(gpio);
}

static uint32_t read_button_state_mask()
{
    uint32_t mask = 0;
    for (size_t i = 0; i < sizeof(WAKE_BUTTON_GPIOS) / sizeof(WAKE_BUTTON_GPIOS[0]); i++) {
        if (is_button_pressed(WAKE_BUTTON_GPIOS[i])) {
            mask |= (1UL << i);
        }
    }
    return mask;
}

static bool is_lora_irq_asserted()
{
    return gpio_get_level(LORA_WAKE_GPIO) != 0;
}


static void pause_button_events_for_sleep()
{
    esp_err_t ret = iot_button_stop();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to stop button timer before sleep: %s", esp_err_to_name(ret));
    }
}

static void resume_button_events_after_sleep()
{
    esp_err_t ret = iot_button_resume();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to resume button timer after sleep: %s", esp_err_to_name(ret));
    }
}

static void wait_for_wake_buttons_release()
{
    while (read_button_state_mask() != 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void enable_light_sleep_wakeup_sources()
{
    for (gpio_num_t gpio : WAKE_BUTTON_GPIOS) {
        gpio_wakeup_disable(gpio);
        ESP_ERROR_CHECK(gpio_wakeup_enable(gpio, board_button_active_level(gpio) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL));
    }

    gpio_intr_disable(LORA_WAKE_GPIO);
    gpio_isr_handler_remove(LORA_WAKE_GPIO);
    gpio_wakeup_disable(LORA_WAKE_GPIO);
    ESP_ERROR_CHECK(gpio_wakeup_enable(LORA_WAKE_GPIO, GPIO_INTR_HIGH_LEVEL));

    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
}

static void disable_light_sleep_wakeup_sources()
{
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);

    for (gpio_num_t gpio : WAKE_BUTTON_GPIOS) {
        gpio_wakeup_disable(gpio);
        gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
    }

    gpio_intr_disable(LORA_WAKE_GPIO);
    gpio_wakeup_disable(LORA_WAKE_GPIO);
    gpio_set_intr_type(LORA_WAKE_GPIO, GPIO_INTR_DISABLE);
    radio_restore_irq_action();
}

static void enter_low_power_sleep(uint32_t *last_rtc_time,
                                  uint32_t *last_bat_time)
{
    ESP_LOGI(TAG, "Idle for %lu ms, entering light sleep", (unsigned long)LOW_POWER_IDLE_TIMEOUT_MS);

    Lora_Transmission_Mode = false;
    Boot_Key_Flag = false;
    lastState = false;
    radio_tx_busy = false;
    radio_irq_fired = false;
    audio_pipeline_clear_all();
    pause_button_events_for_sleep();

    peripheral_lock();
    int radio_state = radio.startReceive();
    peripheral_unlock();
    if (radio_state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "Radio receive before sleep failed, state: %d", radio_state);
    }

    audio_pipeline_prepare_sleep();

    board_display_lock(-1);
    board_display_backlight_off();
    board_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(50));
    enable_light_sleep_wakeup_sources();
    esp_err_t sleep_ret = esp_light_sleep_start();
    bool lora_irq_pending = is_lora_irq_asserted();
    disable_light_sleep_wakeup_sources();

    if (sleep_ret != ESP_OK) {
        ESP_LOGW(TAG, "Light sleep failed: %s", esp_err_to_name(sleep_ret));
    }

    board_display_lock(-1);
    board_display_backlight_on();
    ui_refresh_status_bar();
    board_display_unlock();

    wait_for_wake_buttons_release();
    resume_button_events_after_sleep();

    audio_pipeline_resume_from_sleep();
    audio_pipeline_clear_all();

    Boot_Key_Flag = is_button_pressed(PTT_KEY_GPIO);
    lastState = Boot_Key_Flag;
    Lora_Transmission_Mode = Boot_Key_Flag;

    if (!Boot_Key_Flag && !lora_irq_pending) {
        peripheral_lock();
        radio_state = radio.startReceive();
        peripheral_unlock();
        if (radio_state != RADIOLIB_ERR_NONE) {
            halt_listen_error(radio_state);
        }
    }

    uint32_t now = xTaskGetTickCount();
    *last_rtc_time = now;
    *last_bat_time = now;
    radio_irq_fired = lora_irq_pending;

    request_ptt_status_refresh();
    ESP_LOGI(TAG, "Woke from light sleep%s", lora_irq_pending ? " by LoRa IRQ" : "");
}

static void handle_low_power_idle(uint32_t *last_activity_time,
                                  uint32_t *last_button_state,
                                  uint32_t *last_rtc_time,
                                  uint32_t *last_bat_time)
{
    uint32_t now = xTaskGetTickCount();
    uint32_t button_state = read_button_state_mask();

    if (is_lora_irq_asserted()) {
        radio_irq_fired = true;
        *last_activity_time = now;
        *last_button_state = button_state;
        return;
    }

    if (button_state != *last_button_state || button_state != 0 || radio_tx_busy || Lora_Transmission_Mode) {
        *last_activity_time = now;
        *last_button_state = button_state;
        return;
    }

    if (now - *last_activity_time < pdMS_TO_TICKS(LOW_POWER_IDLE_TIMEOUT_MS)) {
        return;
    }

    enter_low_power_sleep(last_rtc_time, last_bat_time);
    *last_activity_time = xTaskGetTickCount();
    *last_button_state = read_button_state_mask();
}

static bool run_sdcard_test()
{
    static const char *test_file_path = "/sdcard/factory_test.txt";
    static const char test_payload[] = "factory-test\n";

    board_sdcard_mount();

    FILE *fp = fopen(test_file_path, "wb");
    if (!fp) {
        ESP_LOGE(TAG, "SD card test failed: open for write");
        return false;
    }

    size_t written = fwrite(test_payload, 1, sizeof(test_payload) - 1, fp);
    bool ok = (written == sizeof(test_payload) - 1);
    if (fclose(fp) != 0) {
        ok = false;
    }

    if (!ok) {
        ESP_LOGE(TAG, "SD card test failed: write");
        remove(test_file_path);
        return false;
    }

    fp = fopen(test_file_path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "SD card test failed: open for read");
        remove(test_file_path);
        return false;
    }

    char readback[sizeof(test_payload)] = {};
    size_t read_len = fread(readback, 1, sizeof(test_payload) - 1, fp);
    if (fclose(fp) != 0) {
        ok = false;
    }

    ok = ok &&
         read_len == sizeof(test_payload) - 1 &&
         memcmp(readback, test_payload, sizeof(test_payload) - 1) == 0;
    remove(test_file_path);

    if (ok) {
        ESP_LOGI(TAG, "SD card test OK");
    } else {
        ESP_LOGE(TAG, "SD card test failed: readback mismatch");
    }

    return ok;
}

static bool run_wifi_scan_test()
{
    bool ok = false;
    bool event_loop_created = false;
    bool wifi_initialized = false;
    bool wifi_started = false;
    esp_netif_t *sta_netif = NULL;
    esp_err_t ret = ESP_OK;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_scan_config_t scan_config = {};
    uint16_t ap_count = 0;

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "WiFi test failed: esp_netif_init (%s)", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret == ESP_OK) {
        event_loop_created = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "WiFi test failed: event loop (%s)", esp_err_to_name(ret));
        return false;
    }

    sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) {
        ESP_LOGE(TAG, "WiFi test failed: create STA netif");
        goto cleanup;
    }

    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi test failed: init (%s)", esp_err_to_name(ret));
        goto cleanup;
    }
    wifi_initialized = true;

    ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi test failed: set storage (%s)", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi test failed: set STA mode (%s)", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi test failed: start (%s)", esp_err_to_name(ret));
        goto cleanup;
    }
    wifi_started = true;

    scan_config.show_hidden = true;
    ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi test failed: scan (%s)", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi test failed: get AP count (%s)", esp_err_to_name(ret));
        goto cleanup;
    }

    ok = (ap_count > 0);
    ESP_LOGI(TAG, "WiFi scan AP count: %u", (unsigned)ap_count);
    esp_wifi_clear_ap_list();
    if (!ok) {
        ESP_LOGE(TAG, "WiFi test failed: no AP found");
    }

cleanup:
    if (wifi_started) {
        esp_wifi_stop();
    }
    if (sta_netif) {
        esp_netif_destroy_default_wifi(sta_netif);
    }
    if (wifi_initialized) {
        esp_wifi_deinit();
    }
    if (event_loop_created) {
        esp_event_loop_delete_default();
    }

    if (ok) {
        ESP_LOGI(TAG, "WiFi test OK");
    }

    return ok;
}

static void run_factory_tests()
{
    sdcard_test_result = run_sdcard_test() ? FACTORY_TEST_OK : FACTORY_TEST_FAIL;
    refresh_factory_test_status_now();

    wifi_test_result = run_wifi_scan_test() ? FACTORY_TEST_OK : FACTORY_TEST_FAIL;
    refresh_factory_test_status_now();
}

// Bring up shared peripherals in a guarded order: I2C devices first, radio last.
static void hardware_init()
{
    board_display_lock(-1);
    peripheral_lock();
    rtc_ready = board_rtc_init() == ESP_OK;
    bool rtc_ok = rtc_ready && set_rtc_time_to_noon();
    rtc_test_result = factory_result_from_bool(rtc_ok);
    peripheral_unlock();
    board_display_unlock();
    refresh_time_status_now();
    refresh_factory_test_status_now();

    board_display_lock(-1);
    peripheral_lock();
    battery_gauge_ready = board_bat_init(1000) == ESP_OK;
    battery_test_result = battery_gauge_ready ? FACTORY_TEST_PENDING : FACTORY_TEST_FAIL;
    if (battery_gauge_ready) {
        update_battery_info();
    }
    peripheral_unlock();
    board_display_unlock();
    refresh_battery_status_now();
    refresh_factory_test_status_now();

    run_factory_tests();

    peripheral_lock();
    int state = radio_apply_current_config(true);
    if (state != RADIOLIB_ERR_NONE) {
        peripheral_unlock();
        ESP_LOGE(TAG, "Radio init failed, state: %d", state);
        halt_task();
    }
    peripheral_unlock();
    ESP_LOGI(TAG, "Radio modem: %s", radio_modem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa");
}

static void display_init()
{
    if (!board_display_start() || !board_display_lock(-1)) {
        ESP_LOGE(TAG, "Display initialization failed");
        halt_task();
    }
    ui_init();
    board_display_unlock();
}

static void create_tasks()
{
    audio_pipeline_start();
}

static void handle_ptt_key_change()
{
    if (Boot_Key_Flag != lastState) {
        lastState = Boot_Key_Flag;
        
        if (Boot_Key_Flag) {
            // Start a clean TX talk burst and drop stale audio from the previous state.
            Lora_Transmission_Mode = true;
            audio_pipeline_clear_all();
        } else {
            Lora_Transmission_Mode = false;
            if (!radio_tx_busy) {
                peripheral_lock();
                int state = radio.startReceive();
                if (state != RADIOLIB_ERR_NONE) {
                    peripheral_unlock();
                    halt_listen_error(state);
                }
                peripheral_unlock();
            }
            audio_pipeline_clear_all();
        }
        
        request_ptt_status_refresh();
    }
}

static void handle_radio_reconfigure()
{
    if (!radio_reconfigure_requested || radio_tx_busy) {
        return;
    }

    radio_reconfigure_requested = false;
    radio_irq_fired = false;
    audio_pipeline_clear_all();

    peripheral_lock();
    int state = radio_apply_current_config(!Boot_Key_Flag);
    peripheral_unlock();

    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Radio reconfigure failed, state: %d", state);
        halt_task();
    }

    ESP_LOGI(TAG, "Radio modem: %s", radio_modem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa");
}

static void handle_lora_transmission()
{
    if (!Lora_Transmission_Mode) {
        return;
    }

    if (radio_tx_busy) {
        return;
    }
 
    // Transmit only full Codec2 aggregate packets; RX drops any other length.
    if (!audio_pipeline_pop_tx_packet(Send_Receive_Package, LORA_TRANSMISSION_DATA_SIZE)) {
        return;
    }
    
    printf("[LR1121] Sending packet...\r\n");

    peripheral_lock();
    int state = radio.standby();
    if (state == RADIOLIB_ERR_NONE) {
        state = radio.startTransmit(Send_Receive_Package, LORA_TRANSMISSION_DATA_SIZE);
    }
    radio_irq_fired = false;
    if (state == RADIOLIB_ERR_NONE) {
        radio_tx_busy = true;
    } else {
        printf("[LR1121] Transmit failed, code: %d\n", state);
    }
    peripheral_unlock();
}

static void handle_periodic_tasks(uint32_t *last_rtc_time,
                                  uint32_t *last_bat_time)
{
    uint32_t now = xTaskGetTickCount();

    // RTC and battery reads share I2C with the OLED, so they are serialized here.
    if (now - *last_rtc_time >= pdMS_TO_TICKS(1000)) {
        if (board_display_lock(-1)) {
            peripheral_lock();
            bool rtc_ok = read_rtc_time();
            rtc_test_result = factory_result_from_bool(rtc_ok);
            peripheral_unlock();
            ui_refresh_time_status();
            board_display_unlock();
            *last_rtc_time = now;
        }
    }

    if (now - *last_bat_time >= pdMS_TO_TICKS(1000)) {
        *last_bat_time = now;
        if (board_display_lock(-1)) {
            peripheral_lock();
            update_battery_info();
            peripheral_unlock();
            ui_refresh_battery_status();
            ui_refresh_factory_test_status();
            board_display_unlock();
        }
    }
}

static void handle_rx_done()
{
    peripheral_lock();
    size_t raw_payload_len = radio.getPacketLength();
    size_t payload_len = raw_payload_len;
    if (payload_len > sizeof(Send_Receive_Package))
        payload_len = sizeof(Send_Receive_Package);

    if (payload_len == 0) {
        int state = radio.startReceive();
        if (state != RADIOLIB_ERR_NONE) {
            peripheral_unlock();
            halt_listen_error(state);
        }
        peripheral_unlock();
        return;
    }

    int read_state = radio.readData(Send_Receive_Package, payload_len);
    if (read_state == RADIOLIB_ERR_NONE && raw_payload_len == LORA_TRANSMISSION_DATA_SIZE) {
        printf("[LR1121] Packet received! Length: %u bytes\n", (unsigned)raw_payload_len);
        audio_pipeline_push_rx_packet(Send_Receive_Package, payload_len);
    } else {
        printf("[LR1121] Drop packet, len: %u, state: %d\n", (unsigned)raw_payload_len, read_state);
    }

    int state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        peripheral_unlock();
        halt_listen_error(state);
    }
    peripheral_unlock();
}

void lora_irq_process()
{
    radio_irq_fired = false;

    // TX completion owns the IRQ while a packet is on air; RX packets are handled below.
    if (radio_tx_busy) {
        peripheral_lock();
        int state = radio.finishTransmit();
        radio_tx_busy = false;
        if (state != RADIOLIB_ERR_NONE) {
            printf("[LR1121] Finish transmit failed, code: %d\n", state);
        }

        if (!Boot_Key_Flag) {
            state = radio.startReceive();
            if (state != RADIOLIB_ERR_NONE) {
                peripheral_unlock();
                halt_listen_error(state);
            }
        }
        peripheral_unlock();
        return;
    }

    if (Lora_Transmission_Mode) {
        return;
    }
    handle_rx_done();

}

extern "C" void app_main(void)
{
    if (!peripheral_guard_init()) {
        ESP_LOGE(TAG, "Failed to create peripheral mutex");
        halt_task();
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    radio_settings_load();
    display_init();
    hardware_init();
    start_status_timer();
    create_tasks();
    
    uint32_t last_rtc_time = xTaskGetTickCount();
    uint32_t last_bat_time = xTaskGetTickCount();
    uint32_t last_activity_time = xTaskGetTickCount();
    uint32_t last_button_state = read_button_state_mask();
    
    while (1) {
        handle_radio_reconfigure();
        handle_ptt_key_change();
        handle_lora_transmission();
        
        if (radio_irq_fired) {
            lora_irq_process();
        }
        
        handle_periodic_tasks(&last_rtc_time, &last_bat_time);
        handle_low_power_idle(&last_activity_time,
                              &last_button_state,
                              &last_rtc_time,
                              &last_bat_time);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
