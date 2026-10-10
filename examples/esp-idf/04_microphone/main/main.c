/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_extra.h"

static const char *TAG = "microphone_main";

#define FRAME_SIZE 1024
#define PRINT_SAMPLES 16
#define CHANNEL_COUNT 4

static int16_t audio_frame[FRAME_SIZE * CHANNEL_COUNT];

void app_main(void)
{
    esp_err_t ret = board_extra_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize codec: %s", esp_err_to_name(ret));
        return;
    }

    ret = board_extra_codec_gain_set(30, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set codec gain: %s", esp_err_to_name(ret));
        return;
    }

    while (1) {
        size_t bytes_read = 0;

        ret = board_extra_i2s_read(audio_frame,
                                 sizeof(audio_frame),
                                 &bytes_read,
                                 portMAX_DELAY);
        if (ret != ESP_OK || bytes_read != sizeof(audio_frame)) {
            ESP_LOGW(TAG, "I2S read failed or incomplete: ret=%s, bytes_read=%u",
                     esp_err_to_name(ret), (unsigned int)bytes_read);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        char sample_log[192] = {0};
        size_t offset = 0;
        for (int i = 0; i < PRINT_SAMPLES; i++) {
            int16_t left_sample = audio_frame[i * CHANNEL_COUNT + 0];
            int16_t right_sample = audio_frame[i * CHANNEL_COUNT + 2];

            int written = snprintf(&sample_log[offset], sizeof(sample_log) - offset,
                                   "%d,%d | ", left_sample, right_sample);
            if (written < 0 || (size_t)written >= sizeof(sample_log) - offset) {
                break;
            }
            offset += written;
        }
        ESP_LOGI(TAG, "Audio L/R: %s", sample_log);
    }
}
