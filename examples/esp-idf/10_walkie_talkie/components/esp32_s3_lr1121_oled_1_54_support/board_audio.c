// SPDX-License-Identifier: Apache-2.0
// ES8311 playback + ES7210 slot-0 capture share clock pins on this product.
#include "sdkconfig.h"
#if CONFIG_WS_BOARD_AUDIO
#include <assert.h>
#include "board_support.h"
#include "esp_check.h"
#include "esp_board_entry.h"
#include "gen_board_device_custom.h"
#include "periph_i2s.h"
#include "esp_codec_dev_defaults.h"
static const char *TAG = "ws_audio";
#define BSP_NULL_CHECK(p, ret) do { if (!(p)) return (ret); } while (0)
static i2c_master_bus_handle_t i2c_handle;
static int i2c_port;
static const audio_codec_data_if_t *i2s_data_if;
static const dev_custom_audio_config_t *audio_config;
typedef struct { esp_codec_dev_handle_t speaker, microphone; } ws_audio_handles_t;
static ws_audio_handles_t audio;
static esp_codec_dev_handle_t create_speaker(void)
{


    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = audio_config->speaker_address << 1,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = GPIO_NUM_NC,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    BSP_NULL_CHECK(es8311_dev, NULL);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_dev_cfg);
}

static esp_codec_dev_handle_t create_microphone(void)
{


    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = audio_config->microphone_address << 1,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(i2c_ctrl_if, NULL);

    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = i2c_ctrl_if,
    };
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    const audio_codec_if_t *es7210_dev = es7210_codec_new(&es7210_cfg);
    BSP_NULL_CHECK(es7210_dev, NULL);

    esp_codec_dev_cfg_t codec_es7210_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7210_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_es7210_dev_cfg);
}


static int audio_init(void *config, int size, void **handle)
{
    if (size != sizeof(dev_custom_audio_config_t)) return ESP_ERR_INVALID_ARG;
    audio_config = config;
    i2c_handle = board_i2c_get_handle();
    if (!i2c_handle) return ESP_FAIL;
    i2c_master_bus_config_t *bus_cfg;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_periph_config("i2c_main", (void **)&bus_cfg), TAG, "I2C config");
    i2c_port = bus_cfg->i2c_port;
    i2s_chan_handle_t tx = NULL, rx = NULL;
    ESP_RETURN_ON_ERROR(board_peripheral("i2s_speaker", (void **)&tx), TAG, "speaker I2S");
    ESP_RETURN_ON_ERROR(board_peripheral("i2s_microphone", (void **)&rx), TAG, "microphone I2S");
    periph_i2s_config_t *i2s_cfg;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_periph_config("i2s_speaker", (void **)&i2s_cfg), TAG, "I2S config");
    audio_codec_i2s_cfg_t data_cfg = { .port = i2s_cfg->port, .tx_handle = tx, .rx_handle = rx };
    i2s_data_if = audio_codec_new_i2s_data(&data_cfg);
    ESP_RETURN_ON_FALSE(i2s_data_if, ESP_ERR_NO_MEM, TAG, "audio data interface");
    audio.speaker = create_speaker();
    audio.microphone = create_microphone();
    ESP_RETURN_ON_FALSE(audio.speaker && audio.microphone, ESP_FAIL, TAG, "audio codecs");
    ESP_RETURN_ON_ERROR(board_audio_poweramp_enable(true), TAG, "amplifier");
    *handle = &audio;
    return ESP_OK;
}
static int audio_deinit(void *handle) { (void)handle; return ESP_ERR_NOT_SUPPORTED; }
ESP_BOARD_ENTRY_IMPLEMENT(audio, audio_init, audio_deinit);
esp_codec_dev_handle_t board_audio_codec_speaker_init(uint32_t sample_rate)
{
    (void)sample_rate; // Format changes are applied with esp_codec_dev_open().
    ws_audio_handles_t *dev = NULL;
    if (board_device("audio", (void **)&dev) != ESP_OK) return NULL;
    return dev->speaker;
}
esp_codec_dev_handle_t board_audio_codec_microphone_init(uint32_t sample_rate)
{
    (void)sample_rate;
    ws_audio_handles_t *dev = NULL;
    if (board_device("audio", (void **)&dev) != ESP_OK) return NULL;
    return dev->microphone;
}

#endif // CONFIG_WS_BOARD_AUDIO
