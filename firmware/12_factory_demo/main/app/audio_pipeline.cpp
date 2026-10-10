/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio_pipeline.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "board_extra.h"
#include "codec2.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_state.h"
#include "audio_stream_buffer.hpp"

#define AUDIO_BUFFER_SIZE (160 * 20)
#define VOLUME_LEVEL 100
#define GAIN_LEVEL 30
#define TX_START_SKIP_FRAMES 10
#define TX_SILENCE_LEVEL 50
#define TX_AGC_TARGET_LEVEL 2400
#define TX_AGC_MIN_GAIN_Q8 192
#define TX_AGC_MAX_GAIN_Q8 (4 * 256)
#define TX_HIGHPASS_COEFF_Q15 32604
#define TX_PEAK_LIMIT 28000
#define PLAYBACK_GAIN_NUM 4
#define PLAYBACK_GAIN_DEN 5
#define PLAYBACK_PEAK_LIMIT 26000

static const char *TAG = "audio_pipeline";

// Buffers decouple codec, radio and I2S timing so short stalls do not click.
static DynamicBuffer<int16_t> iis_write_buffer(AUDIO_BUFFER_SIZE);
static DynamicBuffer<uint8_t> lora_send_buffer(LORA_TRANSMISSION_DATA_SIZE * 10);
static DynamicBuffer<uint8_t> lora_receive_buffer(LORA_TRANSMISSION_DATA_SIZE * 10);
static volatile bool audio_pipeline_suspended = false;

// TX processor removes DC/rumble, gates silence and applies slow AGC before Codec2.
struct TxAudioProcessor {
    int16_t prev_input = 0;
    int32_t prev_highpass = 0;
    int32_t gain_q8 = 256;

    void reset()
    {
        prev_input = 0;
        prev_highpass = 0;
        gain_q8 = 256;
    }

    bool process(int16_t *samples, size_t sample_count)
    {
        if (!samples || sample_count == 0) return false;

        // First pass: high-pass filter and measure average absolute level.
        uint32_t level_sum = 0;
        for (size_t i = 0; i < sample_count; i++) {
            int32_t input = samples[i];
            int32_t highpass = input - prev_input +
                               ((prev_highpass * TX_HIGHPASS_COEFF_Q15) >> 15);

            prev_input = (int16_t)input;
            prev_highpass = highpass;

            if (highpass > INT16_MAX) highpass = INT16_MAX;
            if (highpass < INT16_MIN) highpass = INT16_MIN;

            samples[i] = (int16_t)highpass;
            level_sum += highpass >= 0 ? highpass : -highpass;
        }

        uint32_t level = level_sum / sample_count;
        if (level < TX_SILENCE_LEVEL) {
            memset(samples, 0, sample_count * sizeof(int16_t));
            gain_q8 = 256;
            return true;
        }

        // Smooth gain changes more slowly upward than downward to avoid pumping noise.
        int32_t target_gain_q8 = (TX_AGC_TARGET_LEVEL * 256) / (int32_t)level;
        if (target_gain_q8 < TX_AGC_MIN_GAIN_Q8) target_gain_q8 = TX_AGC_MIN_GAIN_Q8;
        if (target_gain_q8 > TX_AGC_MAX_GAIN_Q8) target_gain_q8 = TX_AGC_MAX_GAIN_Q8;

        if (target_gain_q8 < gain_q8) {
            gain_q8 = (gain_q8 * 3 + target_gain_q8) / 4;
        } else {
            gain_q8 = (gain_q8 * 7 + target_gain_q8) / 8;
        }

        for (size_t i = 0; i < sample_count; i++) {
            int32_t sample = ((int32_t)samples[i] * gain_q8) >> 8;

            if (sample > TX_PEAK_LIMIT) sample = TX_PEAK_LIMIT;
            if (sample < -TX_PEAK_LIMIT) sample = -TX_PEAK_LIMIT;

            samples[i] = (int16_t)sample;
        }

        return true;
    }
};

static int16_t limit_playback_sample(int32_t sample)
{
    sample = sample * PLAYBACK_GAIN_NUM / PLAYBACK_GAIN_DEN;
    if (sample > PLAYBACK_PEAK_LIMIT) return PLAYBACK_PEAK_LIMIT;
    if (sample < -PLAYBACK_PEAK_LIMIT) return -PLAYBACK_PEAK_LIMIT;
    return (int16_t)sample;
}

static void prepare_playback_samples(int16_t *samples, size_t sample_count)
{
    if (!samples) return;

    for (size_t i = 0; i < sample_count; i++) {
        samples[i] = limit_playback_sample(samples[i]);
    }
}

static void codec2_task(void *parameter)
{
    (void)parameter;

    // One RF packet carries 30 Codec2 3200 frames: 240 bytes / 8 bytes per frame.
    const uint8_t msm_sample_frequency = LORA_TRANSMISSION_DATA_SIZE / 8;

    struct CODEC2 *codec2_state = codec2_create(CODEC2_MODE_3200);
    if (!codec2_state) {
        printf("Failed to create Codec2\r\n");
        vTaskDelete(NULL);
        return;
    }

    codec2_set_lpc_post_filter(codec2_state, true, true, 0.5f, 0.7f);
    int codec2_sample_size = codec2_samples_per_frame(codec2_state);
    int codec2_compress_size = codec2_bytes_per_frame(codec2_state);
    const size_t pcm_frame_bytes = codec2_sample_size * sizeof(int16_t);

    int16_t *msm_sample_buf = (int16_t *)malloc(pcm_frame_bytes);
    int16_t *codec2_buf = (int16_t *)malloc(pcm_frame_bytes);
    uint8_t *codec2_compress_buf = (uint8_t *)malloc(codec2_compress_size);
    bool tx_audio_active = false;
    uint8_t tx_skip_frames = 0;
    TxAudioProcessor tx_processor;

    if (!msm_sample_buf || !codec2_buf || !codec2_compress_buf) {
        printf("Codec2 buffer malloc failed\r\n");
        goto exit;
    }

    printf("Codec2 init OK | sample:%d compress:%d\n",
           codec2_sample_size, codec2_compress_size);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        if (audio_pipeline_suspended) {
            tx_audio_active = false;
            tx_processor.reset();
            continue;
        }

        // PTT active: collect PCM, encode Codec2 frames and queue full radio packets.
        if (Lora_Transmission_Mode) {
            if (!tx_audio_active) {
                tx_audio_active = true;
                tx_skip_frames = TX_START_SKIP_FRAMES;
                tx_processor.reset();
            }

            for (uint8_t i = 0; i < msm_sample_frequency; i++) {
                size_t bytes_read = 0;
                board_extra_i2s_read(msm_sample_buf, pcm_frame_bytes, &bytes_read, portMAX_DELAY);

                if (bytes_read != pcm_frame_bytes) continue;

                if (tx_skip_frames > 0) {
                    tx_skip_frames--;
                    continue;
                }

                if (!tx_processor.process(msm_sample_buf, codec2_sample_size)) {
                    continue;
                }

                codec2_encode(codec2_state, codec2_compress_buf, msm_sample_buf);
                lora_send_buffer.push(codec2_compress_buf, codec2_compress_size);
            }
        } else {
            tx_audio_active = false;
            tx_processor.reset();
        }

        // Decode queued RX frames and hand PCM to the playback task.
        while (true) {
            uint8_t lora_data_buf[LORA_TRANSMISSION_DATA_SIZE];

            if (!lora_receive_buffer.pop(lora_data_buf, codec2_compress_size)) {
                break;
            }

            codec2_decode(codec2_state, codec2_buf, lora_data_buf);
            prepare_playback_samples(codec2_buf, codec2_sample_size);
            iis_write_buffer.push(codec2_buf, codec2_sample_size);
        }
    }

exit:
    free(msm_sample_buf);
    free(codec2_buf);
    free(codec2_compress_buf);
    codec2_destroy(codec2_state);
    vTaskDelete(NULL);
}

// Keep the I2S writer fed with either decoded audio or silence.
static void max_play_task(void *parameter)
{
    (void)parameter;

    static constexpr size_t FRAME_SAMPLES = 160;
    static constexpr size_t FRAME_BYTES = FRAME_SAMPLES * sizeof(int16_t);
    int16_t iis_data_buf[FRAME_SAMPLES];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        if (audio_pipeline_suspended) {
            continue;
        }

        if (!iis_write_buffer.pop(iis_data_buf, FRAME_SAMPLES)) {
            memset(iis_data_buf, 0, FRAME_BYTES);
        }

        size_t bytes_written = 0;
        board_extra_i2s_write(iis_data_buf, FRAME_BYTES, &bytes_written, portMAX_DELAY);
    }
}

void audio_pipeline_start(void)
{
    board_extra_codec_init();
    board_extra_codec_volume_set(VOLUME_LEVEL, nullptr);
    board_extra_codec_gain_set(GAIN_LEVEL, nullptr);

    xTaskCreate(&codec2_task, "Codec2_Task", 32000, NULL, 6, NULL);
    xTaskCreate(&max_play_task, "MAX_Play_Task", 30000, NULL, 6, NULL);
}

// Clear all audio queues when PTT changes state or radio settings are rebuilt.
void audio_pipeline_clear_all(void)
{
    iis_write_buffer.clear();
    lora_send_buffer.clear();
    lora_receive_buffer.clear();
}


esp_err_t audio_pipeline_prepare_sleep(void)
{
    audio_pipeline_suspended = true;
    audio_pipeline_clear_all();
    vTaskDelay(pdMS_TO_TICKS(30));

    esp_err_t ret = board_extra_codec_mute_set(true);
    esp_err_t stop_ret = board_extra_codec_dev_stop();
    if (ret == ESP_OK) {
        ret = stop_ret;
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop codec before sleep: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t audio_pipeline_resume_from_sleep(void)
{
    esp_err_t ret = board_extra_codec_dev_resume();
    esp_err_t step_ret = board_extra_codec_volume_set(VOLUME_LEVEL, nullptr);
    if (ret == ESP_OK) {
        ret = step_ret;
    }

    step_ret = board_extra_codec_gain_set(GAIN_LEVEL, nullptr);
    if (ret == ESP_OK) {
        ret = step_ret;
    }

    step_ret = board_extra_codec_mute_set(false);
    if (ret == ESP_OK) {
        ret = step_ret;
    }

    audio_pipeline_clear_all();
    audio_pipeline_suspended = false;

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to resume codec after sleep: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool audio_pipeline_pop_tx_packet(uint8_t *packet, size_t packet_size)
{
    if (!packet || packet_size != LORA_TRANSMISSION_DATA_SIZE) {
        return false;
    }

    return lora_send_buffer.pop(packet, LORA_TRANSMISSION_DATA_SIZE);
}

void audio_pipeline_push_rx_packet(const uint8_t *packet, size_t packet_size)
{
    if (!packet || packet_size != LORA_TRANSMISSION_DATA_SIZE) {
        return;
    }

    lora_receive_buffer.push(packet, LORA_TRANSMISSION_DATA_SIZE);
}
