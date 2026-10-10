/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_log.h"
#include "board_support.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_extra.h"

static const char *TAG = "speaker_main";

#define MAX_MP3_FILES UINT8_MAX

static char *mp3_path[MAX_MP3_FILES] = {0};
static char *mp3_filename[MAX_MP3_FILES] = {0};
static uint8_t mp3_num = 0;
static uint8_t mp3_volume = CODEC_DEFAULT_VOLUME;
static uint8_t current_mp3_index = 0;
static bool is_pause = false;
static bool is_volume = false;

static bool btn_next_flag = false;
static bool btn_prev_flag = false;
static bool btn_play_pause_flag = false;
static bool btn_volume_flag = false;


static void button_event_cb(void *arg, void *data)
{
    int button_id = (int)(intptr_t)data;
    button_event_t event = iot_button_get_event(arg);

    if (BUTTON_SINGLE_CLICK == event) {
        switch (button_id) {
        case BOOT_KEY_GPIO:
            btn_next_flag = true;
            break;
        case PWR_KEY_GPIO:
            btn_prev_flag = true;
            break;
        case MENU_KEY_GPIO:
            btn_play_pause_flag = true;
            break;
        case PTT_KEY_GPIO:
            btn_volume_flag = true;
            is_volume = true;
            break;
        default:
            break;
        }
    } else if (BUTTON_DOUBLE_CLICK == event) {
        if (button_id == PTT_KEY_GPIO) {
            btn_volume_flag = true;
            is_volume = false;
        }
    }
}

static esp_err_t play_track(uint8_t index, bool keep_pause_state)
{
    current_mp3_index = index;
    esp_err_t ret = board_extra_player_play_file(mp3_path[current_mp3_index]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to play %s: %s", mp3_path[current_mp3_index], esp_err_to_name(ret));
        return ret;
    }

    if (keep_pause_state && is_pause) {
        audio_player_pause();
    }

    return ESP_OK;
}

static esp_err_t play_next_track(bool keep_pause_state)
{
    uint8_t next_index = current_mp3_index + 1;
    if (next_index >= mp3_num) {
        next_index = 0;
    }

    return play_track(next_index, keep_pause_state);
}

static esp_err_t play_previous_track(void)
{
    uint8_t prev_index = (current_mp3_index == 0) ? (mp3_num - 1) : (current_mp3_index - 1);
    return play_track(prev_index, true);
}

static void speaker_callback(audio_player_cb_ctx_t *ctx)
{
    if (ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_IDLE) {
        (void)play_next_track(false);
    }
}

static void list_files(const char *base_path)
{
    uint8_t i = 0;
    DIR *dir = opendir(base_path);

    if (!dir) {
        ESP_LOGE(TAG, "Could not open directory %s", base_path);
        return;
    }

    struct dirent *entry;
    ESP_LOGI(TAG, "Scanning directory: %s", base_path);

    while (i < MAX_MP3_FILES && (entry = readdir(dir)) != NULL) {
        const char *file_name = entry->d_name;
        size_t len = strlen(file_name);

        if (len > 4 && strcasecmp(&file_name[len - 4], ".mp3") == 0) {
            size_t name_len = len - 4;
            size_t path_len = strlen(base_path) + len + 2;

            char *filename = malloc(name_len + 1);
            char *path = malloc(path_len);

            if (!filename || !path) {
                free(filename);
                free(path);
                ESP_LOGE(TAG, "Memory allocation failed while scanning %s", file_name);
                break;
            }

            snprintf(path, path_len, "%s/%s", base_path, file_name);
            snprintf(filename, name_len + 1, "%.*s", (int)name_len, file_name);

            mp3_path[i] = path;
            mp3_filename[i] = filename;

            ESP_LOGI(TAG, "Found MP3 [%u]: %s", i, mp3_filename[i]);
            i++;
        }
    }

    mp3_num = i;
    closedir(dir);
    ESP_LOGI(TAG, "Scan done. Total MP3: %u", mp3_num);
}

static esp_err_t register_button(int gpio_num, int active_level, bool double_click)
{
    (void)active_level; // Active level belongs to Board Manager YAML.
    button_handle_t btn = board_button_for_gpio((gpio_num_t)gpio_num);
    if (!btn) return ESP_FAIL;
    esp_err_t ret;
    ret = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
    if (ret != ESP_OK) {
        return ret;
    }
    if (double_click) {
        ret = iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret = board_sdcard_mount();
    if (ret == ESP_OK) {
        list_files(BSP_SD_MOUNT_POINT"/music");
        if (mp3_num == 0) {
            ESP_LOGI(TAG, "No MP3 files found!");
            return;
        }
    } else {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return;
    }

    ret = board_extra_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize codec: %s", esp_err_to_name(ret));
        return;
    }

    ret = board_extra_codec_volume_set(mp3_volume, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set volume: %s", esp_err_to_name(ret));
        return;
    }

    board_extra_player_register_callback(speaker_callback, NULL);

    ret = board_extra_player_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize player: %s", esp_err_to_name(ret));
        return;
    }

    ret = board_extra_player_play_file(mp3_path[current_mp3_index]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to play initial file %s: %s", mp3_path[current_mp3_index], esp_err_to_name(ret));
        return;
    }

    const struct {
        int gpio_num;
        int active_level;
        bool double_click;
    } buttons[] = {
        {BOOT_KEY_GPIO, BUTTON_ACTIVE_LEVEL, false},
        {MENU_KEY_GPIO, BUTTON_ACTIVE_LEVEL, false},
        {PTT_KEY_GPIO, BUTTON_ACTIVE_LEVEL, true},
        {PWR_KEY_GPIO, WS_POWER_ACTIVE_LEVEL, false},
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        ret = register_button(buttons[i].gpio_num, buttons[i].active_level, buttons[i].double_click);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register GPIO %d button: %s", buttons[i].gpio_num, esp_err_to_name(ret));
            return;
        }
    }

    ESP_LOGI(TAG, "System ready, waiting key input...");

    while (1) {
        if (btn_next_flag) {
            btn_next_flag = false;
            if (play_next_track(true) == ESP_OK) {
                ESP_LOGI(TAG, "Switched to Next: %s (Pause: %d)", mp3_filename[current_mp3_index], is_pause);
            }
        }

        if (btn_prev_flag) {
            btn_prev_flag = false;
            if (play_previous_track() == ESP_OK) {
                ESP_LOGI(TAG, "Switched to Prev: %s (Pause: %d)", mp3_filename[current_mp3_index], is_pause);
            }
        }

        if (btn_play_pause_flag) {
            btn_play_pause_flag = false;
            is_pause = !is_pause;
            if (is_pause) {
                audio_player_pause();
                ESP_LOGI(TAG, "Playback Paused");
            } else {
                audio_player_resume();
                ESP_LOGI(TAG, "Playback Resumed");
            }
        }

        if (btn_volume_flag) {
            btn_volume_flag = false;
            if (is_volume) {
                mp3_volume = (mp3_volume > 95) ? 100 : (mp3_volume + 5);
            } else {
                mp3_volume = (mp3_volume < 5) ? 0 : (mp3_volume - 5);
            }
            ret = board_extra_codec_volume_set(mp3_volume, NULL);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set volume %u: %s", mp3_volume, esp_err_to_name(ret));
            }
        }
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
}
