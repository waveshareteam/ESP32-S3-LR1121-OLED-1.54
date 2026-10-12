/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#if CONFIG_WS_BOARD_AUDIO
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_codec_dev_defaults.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"

#include "board_extra.h"
#include "gen_board_device_custom.h"
#include "periph_i2s.h"

static const char *TAG = "bsp_extra_board";

static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;
static i2s_chan_handle_t play_i2s_handle;
static i2s_chan_handle_t record_i2s_handle;
static uint32_t audio_sample_rate;
static periph_i2s_config_t microphone_config;
static bool audio_channels_ready;

// Codec2 carries mono 8 kHz speech. Keep wiring from the online board while
// selecting the application's stream format through Board Manager's init hook.
#define WALKIE_SAMPLE_RATE 8000
static esp_err_t walkie_i2s_init(void *config, int size, void **handle)
{
    ESP_RETURN_ON_FALSE(config && size == sizeof(periph_i2s_config_t), ESP_ERR_INVALID_ARG, TAG, "I2S config size");
    periph_i2s_config_t format = *(const periph_i2s_config_t *)config;
    if (format.direction == I2S_DIR_TX && format.mode == I2S_COMM_MODE_STD) {
        format.i2s_cfg.std.clk_cfg.sample_rate_hz = WALKIE_SAMPLE_RATE;
    } else if (format.direction == I2S_DIR_RX && format.mode == I2S_COMM_MODE_TDM) {
        format.i2s_cfg.tdm.clk_cfg.sample_rate_hz = WALKIE_SAMPLE_RATE;
        format.i2s_cfg.tdm.slot_cfg.total_slot = 2;
        format.i2s_cfg.tdm.slot_cfg.slot_mask = I2S_TDM_SLOT0;
        microphone_config = format;
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    return periph_i2s_init(&format, sizeof(format), handle);
}

static esp_err_t walkie_audio_channels_init(void)
{
    if (audio_channels_ready) return ESP_OK;
    ESP_RETURN_ON_ERROR(esp_board_periph_init_custom("i2s_speaker", walkie_i2s_init, periph_i2s_deinit),
                        TAG, "Speaker stream format");
    esp_err_t err = esp_board_periph_init_custom("i2s_microphone", walkie_i2s_init, periph_i2s_deinit);
    if (err != ESP_OK) {
        esp_board_periph_deinit("i2s_speaker");
        return err;
    }
    audio_channels_ready = true;
    return ESP_OK;
}

static bool _is_audio_init = false;
static bool _is_player_init = false;
static int _volume_intensity = CODEC_DEFAULT_VOLUME;
static int _gain_intensity = CODEC_DEFAULT_ADC_VOLUME;

static audio_player_cb_t audio_idle_callback = NULL;
static void *audio_idle_cb_user_data = NULL;
static char audio_file_path[128];

static esp_err_t store_audio_file_path(const char *file_path)
{
    int written = snprintf(audio_file_path, sizeof(audio_file_path), "%s", file_path);
    ESP_RETURN_ON_FALSE(written >= 0 && (size_t)written < sizeof(audio_file_path),
                        ESP_ERR_INVALID_SIZE, TAG, "Audio file path is too long: %s", file_path);
    return ESP_OK;
}

/**************************************************************************************************
 *
 * Extra Board Function
 *
 **************************************************************************************************/

static esp_err_t audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    // Volume saved when muting and restored when unmuting. Restoring volume is necessary
    // as es8311_set_voice_mute(true) results in voice volume (REG32) being set to zero.

    ESP_RETURN_ON_ERROR(board_extra_codec_mute_set(setting == AUDIO_PLAYER_MUTE), TAG, "Set mute failed");

    // restore the voice volume upon unmuting
    if (setting == AUDIO_PLAYER_UNMUTE) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, _volume_intensity), TAG, "Set Codec volume failed");
    }

    return ESP_OK;
}

static void audio_callback(audio_player_cb_ctx_t *ctx)
{
    if (audio_idle_callback) {
        ctx->user_ctx = audio_idle_cb_user_data;
        audio_idle_callback(ctx);
    }
}

esp_err_t board_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    if (bytes_read) *bytes_read = 0;
    ESP_RETURN_ON_FALSE(record_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Record codec is not initialized");
    ESP_RETURN_ON_FALSE(audio_buffer, ESP_ERR_INVALID_ARG, TAG, "audio_buffer is NULL");

    // Use the Board Manager channel so the caller's timeout and partial count
    // are preserved; esp_codec_dev_read uses its own fixed timeout.
    size_t count = 0;
    esp_err_t ret = i2s_channel_read(record_i2s_handle, audio_buffer, len, &count, timeout_ms);
    if (bytes_read) *bytes_read = count;
    return ret;
}

esp_err_t board_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    if (bytes_written) *bytes_written = 0;
    ESP_RETURN_ON_FALSE(play_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Playback codec is not initialized");
    ESP_RETURN_ON_FALSE(audio_buffer, ESP_ERR_INVALID_ARG, TAG, "audio_buffer is NULL");

    size_t count = 0;
    esp_err_t ret = i2s_channel_write(play_i2s_handle, audio_buffer, len, &count, timeout_ms);
    if (bytes_written) *bytes_written = count;
    return ret;
}

esp_err_t board_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (play_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(play_dev_handle), TAG, "Close playback codec failed");
    }
    if (record_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(record_dev_handle), TAG, "Close record codec failed");
    }

    if (play_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(play_dev_handle, &fs), TAG, "Open playback codec failed");
    }
    if (record_dev_handle) {
        // Two physical slots share the speaker clock; DMA returns only slot 0.
        // An explicit mask also keeps ES7210's TDM path at 16 bits per sample.
        fs.channel = microphone_config.i2s_cfg.tdm.slot_cfg.total_slot;
        fs.channel_mask = microphone_config.i2s_cfg.tdm.slot_cfg.slot_mask;
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(record_dev_handle, &fs), TAG, "Open record codec failed");
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME), TAG, "Set record gain failed");
    }
    return ESP_OK;
}

esp_err_t board_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, volume), TAG, "Set Codec volume failed");
    _volume_intensity = volume;
    if (volume_set) {
        *volume_set = _volume_intensity;
    }

    ESP_LOGI(TAG, "Setting volume: %d", volume);

    return ESP_OK;
}

int board_extra_codec_volume_get(void)
{
    return _volume_intensity;
}

esp_err_t board_extra_codec_gain_set(int gain, int *gain_set)
{
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_in_gain(record_dev_handle, gain), TAG, "Set Codec gain failed");
    _gain_intensity = gain;
    if (gain_set) {
        *gain_set = _gain_intensity;
    }

    ESP_LOGI(TAG, "Setting gain: %d", gain);

    return ESP_OK;
}

int board_extra_codec_gain_get(void)
{
    return _gain_intensity;
}

esp_err_t board_extra_codec_mute_set(bool enable)
{
    ESP_RETURN_ON_FALSE(play_dev_handle, ESP_ERR_INVALID_STATE, TAG, "Playback codec is not initialized");
    return esp_codec_dev_set_out_mute(play_dev_handle, enable);
}

esp_err_t board_extra_codec_dev_stop(void)
{
    if (play_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(play_dev_handle), TAG, "Close playback codec failed");
    }

    if (record_dev_handle) {
        ESP_RETURN_ON_ERROR(esp_codec_dev_close(record_dev_handle), TAG, "Close record codec failed");
    }
    return ESP_OK;
}

esp_err_t board_extra_codec_dev_resume(void)
{
    ESP_RETURN_ON_FALSE(audio_sample_rate, ESP_ERR_INVALID_STATE, TAG, "Audio is not initialized");
    return board_extra_codec_set_fs(audio_sample_rate, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
}

uint32_t board_extra_codec_sample_rate_get(void)
{
    return audio_sample_rate;
}

esp_err_t board_extra_codec_init(void)
{
    if (_is_audio_init) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(walkie_audio_channels_init(), TAG, "Walkie-talkie stream format");
    audio_sample_rate = WALKIE_SAMPLE_RATE;

    play_dev_handle = board_audio_codec_speaker_init(audio_sample_rate);
    ESP_RETURN_ON_FALSE(play_dev_handle, ESP_ERR_NOT_FOUND, TAG, "play_dev_handle not initialized");

    record_dev_handle = board_audio_codec_microphone_init(audio_sample_rate);
    ESP_RETURN_ON_FALSE(record_dev_handle, ESP_ERR_NOT_FOUND, TAG, "record_dev_handle not initialized");

    ESP_RETURN_ON_ERROR(board_peripheral("i2s_speaker", (void **)&play_i2s_handle), TAG, "Speaker channel");
    ESP_RETURN_ON_ERROR(board_peripheral("i2s_microphone", (void **)&record_i2s_handle), TAG, "Microphone channel");

    ESP_RETURN_ON_ERROR(board_extra_codec_dev_resume(),
                        TAG, "Set codec format failed");

    _is_audio_init = true;

    return ESP_OK;
}

esp_err_t board_extra_player_init(void)
{
    if (_is_player_init) {
        return ESP_OK;
    }

    audio_player_config_t config = { .mute_fn = audio_mute_function,
                                     .write_fn = board_extra_i2s_write,
                                     .clk_set_fn = board_extra_codec_set_fs,
                                     .priority = 5
    };
    ESP_RETURN_ON_ERROR(audio_player_new(config), TAG, "audio_player_init failed");
    ESP_RETURN_ON_ERROR(audio_player_callback_register(audio_callback, NULL), TAG, "audio_player_callback_register failed");

    _is_player_init = true;

    return ESP_OK;
}

esp_err_t board_extra_player_del(void)
{
    _is_player_init = false;

    ESP_RETURN_ON_ERROR(audio_player_delete(), TAG, "audio_player_delete failed");

    return ESP_OK;
}

esp_err_t board_extra_file_instance_init(const char *path, file_iterator_instance_t **ret_instance)
{
    ESP_RETURN_ON_FALSE(path, ESP_FAIL, TAG, "path is NULL");
    ESP_RETURN_ON_FALSE(ret_instance, ESP_FAIL, TAG, "ret_instance is NULL");

    file_iterator_instance_t *file_iterator = file_iterator_new(path);
    ESP_RETURN_ON_FALSE(file_iterator, ESP_FAIL, TAG, "file_iterator_new failed, %s", path);

    *ret_instance = file_iterator;

    return ESP_OK;
}

esp_err_t board_extra_player_play_index(file_iterator_instance_t *instance, int index)
{
    ESP_RETURN_ON_FALSE(instance, ESP_FAIL, TAG, "instance is NULL");

    ESP_LOGI(TAG, "play_index(%d)", index);
    char filename[128];
    int retval = file_iterator_get_full_path_from_index(instance, index, filename, sizeof(filename));
    ESP_RETURN_ON_FALSE(retval != 0, ESP_FAIL, TAG, "file_iterator_get_full_path_from_index failed");

    ESP_LOGI(TAG, "opening file '%s'", filename);
    FILE *fp = fopen(filename, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_RETURN_ON_ERROR(store_audio_file_path(filename), TAG, "Store audio file path failed");

    ESP_LOGI(TAG, "Playing '%s'", filename);
    ESP_RETURN_ON_ERROR(audio_player_play(fp), TAG, "audio_player_play failed");

    return ESP_OK;
}

esp_err_t board_extra_player_play_file(const char *file_path)
{
    ESP_RETURN_ON_FALSE(file_path, ESP_ERR_INVALID_ARG, TAG, "file_path is NULL");
    ESP_RETURN_ON_ERROR(store_audio_file_path(file_path), TAG, "Store audio file path failed");

    ESP_LOGI(TAG, "opening file '%s'", file_path);
    FILE *fp = fopen(file_path, "rb");
    ESP_RETURN_ON_FALSE(fp, ESP_FAIL, TAG, "unable to open file");

    ESP_LOGI(TAG, "Playing '%s'", file_path);
    ESP_RETURN_ON_ERROR(audio_player_play(fp), TAG, "audio_player_play failed");

    return ESP_OK;
}

void board_extra_player_register_callback(audio_player_cb_t cb, void *user_data)
{
    audio_idle_callback = cb;
    audio_idle_cb_user_data = user_data;
}

bool board_extra_player_is_playing_by_path(const char *file_path)
{
    return (strcmp(audio_file_path, file_path) == 0);
}

bool board_extra_player_is_playing_by_index(file_iterator_instance_t *instance, int index)
{
    return (index == file_iterator_get_index(instance));
}

#endif // CONFIG_WS_BOARD_AUDIO
