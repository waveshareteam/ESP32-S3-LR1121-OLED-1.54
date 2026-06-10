#include <Arduino.h>
#include "driver/i2s_std.h"
#include <esp_vad.h>
#include "es7210.h"
#include "es8311.h"
#include "esp_check.h"

// ==================== Pin ====================
#define PIN_ES7210_BCLK       GPIO_NUM_18
#define PIN_ES7210_LRCK       GPIO_NUM_21
#define PIN_ES7210_DIN        GPIO_NUM_15
#define I2S_DOUT              GPIO_NUM_16
#define PIN_ES7210_MCLK       GPIO_NUM_17
#define PIN_I2C_SDA           GPIO_NUM_13
#define PIN_I2C_SCL           GPIO_NUM_14
#define PIN_PA_CTRL           8

// ==================== Audio ====================
#define SAMPLE_RATE           8000
#define FRAME_MS              30
#define FRAME_SAMPLES         (SAMPLE_RATE * FRAME_MS / 1000)
#define CHANNELS              1
#define BUFFER_SAMPLES        (FRAME_SAMPLES * CHANNELS)

#define MCLK_MULTIPLE         256
#define MCLK_FREQ             (SAMPLE_RATE * MCLK_MULTIPLE)

#define I2S_CH                I2S_NUM_1

// ==================== Buffer ====================
int16_t *i2s_raw_buffer;
int16_t *tx_buffer;
int16_t *vad_buffer;

vad_handle_t vad_inst;

static i2s_chan_handle_t i2s_tx_chan;
static i2s_chan_handle_t i2s_rx_chan;

// ==================== ES8311 ====================
static esp_err_t es8311_codec_init(void)
{
  es8311_handle_t es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);

  const es8311_clock_config_t clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = MCLK_FREQ,
    .sample_frequency = SAMPLE_RATE
  };

  ESP_ERROR_CHECK(es8311_init(es, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
  es8311_sample_frequency_config(es, MCLK_FREQ, SAMPLE_RATE);
  es8311_voice_mute(es, false);
  es8311_voice_volume_set(es, 60, NULL);
  es8311_microphone_gain_set(es, ES8311_MIC_GAIN_MIN);       // 关闭内部麦，避免干扰

  return ESP_OK;
}

// ==================== I2S Config ====================
#define I2S_GPIO_CFG { \
  .mclk = PIN_ES7210_MCLK, \
  .bclk = PIN_ES7210_BCLK, \
  .ws   = PIN_ES7210_LRCK, \
  .dout = I2S_DOUT, \
  .din  = PIN_ES7210_DIN, \
}

#define I2S_STD_CFG_STEREO(_rate) { \
  .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(_rate), \
  .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO), \
  .gpio_cfg = I2S_GPIO_CFG, \
}

// ==================== Setup ====================
void setup()
{
  Serial.begin(115200);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  i2s_raw_buffer = (int16_t *)malloc(BUFFER_SAMPLES * sizeof(int16_t));
  tx_buffer      = (int16_t *)malloc(BUFFER_SAMPLES * sizeof(int16_t));
  vad_buffer     = (int16_t *)malloc(FRAME_SAMPLES * sizeof(int16_t));

  if (!i2s_raw_buffer || !tx_buffer || !vad_buffer) {
    Serial.println("malloc failed");
    while (1);
  }

  // ==================== ES7210 ADC ====================
  audio_hal_codec_config_t cfg = {
    .adc_input = AUDIO_HAL_ADC_INPUT_ALL,
    .codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE,
    .i2s_iface = {
      .mode = AUDIO_HAL_MODE_SLAVE,
      .fmt = AUDIO_HAL_I2S_NORMAL,
      .samples = AUDIO_HAL_08K_SAMPLES,
      .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
    },
  };

  es7210_adc_init(&Wire, &cfg);
  es7210_adc_config_i2s(cfg.codec_mode, &cfg.i2s_iface);
  es7210_adc_set_gain((es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2), GAIN_24DB);
  es7210_adc_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);

  // ==================== ES8311 ====================
  es8311_codec_init();
  pinMode(PIN_PA_CTRL, OUTPUT);
  digitalWrite(PIN_PA_CTRL, 1);

  // ==================== I2S ====================
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_CH, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan);

  i2s_std_config_t std_cfg = I2S_STD_CFG_STEREO(SAMPLE_RATE);
  i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg);
  i2s_channel_enable(i2s_tx_chan);

  i2s_channel_init_std_mode(i2s_rx_chan, &std_cfg);
  i2s_channel_enable(i2s_rx_chan);

  vad_inst = vad_create(VAD_MODE_4);

  Serial.println("Init OK");
}

// ==================== Loop ====================
void loop()
{
  size_t bytes_read, bytes_write;

  i2s_channel_read(
    i2s_rx_chan,
    i2s_raw_buffer,
    BUFFER_SAMPLES * sizeof(int16_t),
    &bytes_read,
    portMAX_DELAY
  );

  // ==================== 选择播放声道 ====================
  #define PLAY_MIC 2    // 1=左MIC  2=右MIC

  for (int i = 0; i < FRAME_SAMPLES; i++) {
    int16_t sample;

    #if PLAY_MIC == 1
      sample = i2s_raw_buffer[i * 2 + 0]; // 左
    #else
      sample = i2s_raw_buffer[i * 2 + 1]; // 右
    #endif

    tx_buffer[i * 2 + 0] = sample;
    tx_buffer[i * 2 + 1] = sample;
    vad_buffer[i] = sample;
  }

  i2s_channel_write(
    i2s_tx_chan,
    i2s_raw_buffer,
    BUFFER_SAMPLES * sizeof(int16_t),
    &bytes_write,
    portMAX_DELAY
  );

  vad_state_t state = vad_process(vad_inst, vad_buffer, SAMPLE_RATE, FRAME_MS);
  if (state == VAD_SPEECH) {
    Serial.println("SPEECH");
  }
}