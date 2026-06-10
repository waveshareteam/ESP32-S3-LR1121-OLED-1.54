#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_board_extra.h"

static const char *TAG = "main";

#define MAX_MP3_FILES UINT8_MAX

static char *Mp3Path[MAX_MP3_FILES] = {0};
static char *Mp3Filename[MAX_MP3_FILES] = {0};
static uint8_t mp3_num = 0;
static uint8_t mp3_volume = CODEC_DEFAULT_VOLUME;
static uint8_t current_mp3_index = 0;
static bool is_pause = false;
static bool is_volume = false;

static bool btn_next_flag = false;
static bool btn_prev_flag = false;
static bool btn_play_pause_flag = false;
static bool btn_volume_flag = false;

#define UP_IO_NUM        0
#define DOWN_IO_NUM      4
#define RESUME_IO_NUM    5
#define VOLUME_IO_NUM    6
#define BUTTON_ACTIVE_LEVEL  0
#define DOWN_ACTIVE_LEVEL  1

static void button_event_cb(void *arg, void *data)
{
    int button_id = (int)(intptr_t)data;
    button_event_t event = iot_button_get_event(arg);

    if (BUTTON_SINGLE_CLICK == event) {
        switch (button_id) {
        case UP_IO_NUM:
            btn_next_flag = true;
            break;
        case DOWN_IO_NUM:
            btn_prev_flag = true;
            break;
        case RESUME_IO_NUM:
            btn_play_pause_flag = true;
            break;
        case VOLUME_IO_NUM:
            btn_volume_flag = true;
            is_volume = true;
            break;
        default:
            break;
        }
    } else if (BUTTON_DOUBLE_CLICK == event) {
        if (button_id == VOLUME_IO_NUM) {
            btn_volume_flag = true;
            is_volume = false;
        }
    }
}

static void play_track(uint8_t index, bool keep_pause_state)
{
    current_mp3_index = index;
    bsp_extra_player_play_file(Mp3Path[current_mp3_index]);

    if (keep_pause_state && is_pause) {
        audio_player_pause();
    }
}

static void play_next_track(bool keep_pause_state)
{
    uint8_t next_index = current_mp3_index + 1;
    if (next_index >= mp3_num) {
        next_index = 0;
    }

    play_track(next_index, keep_pause_state);
}

static void play_previous_track(void)
{
    uint8_t prev_index = (current_mp3_index == 0) ? (mp3_num - 1) : (current_mp3_index - 1);
    play_track(prev_index, true);
}

void speaker_callback(audio_player_cb_ctx_t *ctx)
{
    if (ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_IDLE) {
        play_next_track(false);
    }
}

static void list_files(const char *base_path)
{
    uint8_t i = 0;
    DIR *dir = opendir(base_path);

    if (!dir) {
        printf("Error: Could not open directory %s\n", base_path);
        return;
    }

    struct dirent *entry;
    printf("=== Scanning directory: %s ===\n", base_path);

    while (i < MAX_MP3_FILES && (entry = readdir(dir)) != NULL) {
        const char *file_name = entry->d_name;
        size_t len = strlen(file_name);

        if (len > 4 && strcasecmp(&file_name[len - 4], ".mp3") == 0) {
            size_t name_len = len - 4;
            size_t path_len = strlen(base_path) + len + 2;

            Mp3Filename[i] = malloc(name_len + 1);
            Mp3Path[i] = malloc(path_len);

            if (!Mp3Filename[i] || !Mp3Path[i]) {
                printf("Memory allocation failed!\n");
                break;
            }

            snprintf(Mp3Path[i], path_len, "%s/%s", base_path, file_name);
            snprintf(Mp3Filename[i], name_len + 1, "%.*s", (int)name_len, file_name);

            printf("Found MP3 [%d]: %s\n", i, Mp3Filename[i]);
            i++;
        }
    }

    mp3_num = i;
    closedir(dir);
    printf("=== Scan done! Total MP3: %d ===\n", mp3_num);
}

static esp_err_t register_button(int gpio_num, int active_level, bool double_click)
{
    const button_config_t btn_cfg = {0};
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = gpio_num,
        .active_level = active_level,
    };
    button_handle_t btn = NULL;

    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    if (ret != ESP_OK) {
        return ret;
    }

    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
    if (double_click) {
        iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, NULL, button_event_cb, (void *)(intptr_t)gpio_num);
    }

    return ESP_OK;
}

void app_main(void)
{
    if (bsp_sdcard_mount() == ESP_OK) {
        list_files(BSP_SD_MOUNT_POINT"/music");
        if (mp3_num == 0) {
            ESP_LOGI(TAG, "No MP3 files found!");
            return;
        }
    }

    bsp_extra_codec_init();
    bsp_extra_codec_volume_set(mp3_volume, NULL);
    bsp_extra_player_register_callback(speaker_callback, NULL);
    bsp_extra_player_init();

    bsp_extra_player_play_file(Mp3Path[current_mp3_index]);

    const struct {
        int gpio_num;
        int active_level;
        bool double_click;
    } buttons[] = {
        {UP_IO_NUM, BUTTON_ACTIVE_LEVEL, false},
        {RESUME_IO_NUM, BUTTON_ACTIVE_LEVEL, false},
        {VOLUME_IO_NUM, BUTTON_ACTIVE_LEVEL, true},
        {DOWN_IO_NUM, DOWN_ACTIVE_LEVEL, false},
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        if (register_button(buttons[i].gpio_num, buttons[i].active_level, buttons[i].double_click) != ESP_OK) {
            return;
        }
    }

    ESP_LOGI(TAG, "System ready, waiting key input...");

    while (1) {
        if (btn_next_flag) {
            btn_next_flag = false;
            play_next_track(true);
            ESP_LOGI(TAG, "Switched to Next: %s (Pause: %d)", Mp3Filename[current_mp3_index], is_pause);
        }

        if (btn_prev_flag) {
            btn_prev_flag = false;
            play_previous_track();
            ESP_LOGI(TAG, "Switched to Prev: %s (Pause: %d)", Mp3Filename[current_mp3_index], is_pause);
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
                mp3_volume += 5;
                if (mp3_volume > 100) {
                    mp3_volume = 100;
                }
            } else {
                mp3_volume -= 5;
                if (mp3_volume < 0) {
                    mp3_volume = 0;
                }
            }
            bsp_extra_codec_volume_set(mp3_volume, NULL);
        }
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
}
