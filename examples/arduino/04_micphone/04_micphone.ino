/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "driver/i2s_tdm.h"
#include "esp_err.h"
#include "Config.h"
#include "CodecDrivers.h"
#include "Wire.h"

#define I2S_CH                I2S_NUM_1

// Audio format used by examples/esp-idf/04_micphone/main/main.c.
#define FRAME_SIZE            1024
#define PRINT_SAMPLES         16
#define CHANNEL_COUNT         AUDIO_MIC_TDM_TOTAL_SLOTS
#define SAMPLE_BITS           16
#define RX_TDM_SLOT_MASK      ((i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3))

static int16_t audio_frame[FRAME_SIZE * CHANNEL_COUNT];
static i2s_chan_handle_t i2s_rx_chan;
static Es7210Adc es7210;

// Print the same logical microphone pair used by the ESP-IDF example.
static void printAudioFrame()
{
  Serial.print("Audio L/R: ");
  for (int i = 0; i < PRINT_SAMPLES; i++) {
    int16_t left_sample = audio_frame[i * CHANNEL_COUNT + 0];
    int16_t right_sample = audio_frame[i * CHANNEL_COUNT + 2];
    Serial.printf("%d,%d | ", left_sample, right_sample);
  }
  Serial.println();
}

// Configure the RX side of I2S for four-slot TDM microphone capture.
static void initI2sRx()
{
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_CH, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &i2s_rx_chan));

  i2s_tdm_config_t rx_cfg = {};
  rx_cfg.clk_cfg.sample_rate_hz = AUDIO_SAMPLE_RATE;
  rx_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
  rx_cfg.clk_cfg.ext_clk_freq_hz = 0;
  rx_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  rx_cfg.clk_cfg.bclk_div = 8;

  rx_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
  rx_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
  rx_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
  rx_cfg.slot_cfg.slot_mask = RX_TDM_SLOT_MASK;
  // Match the final TDM WS width configured by esp_codec_dev in the IDF BSP.
  rx_cfg.slot_cfg.ws_width = SAMPLE_BITS * CHANNEL_COUNT / 2;
  rx_cfg.slot_cfg.ws_pol = false;
  rx_cfg.slot_cfg.bit_shift = true;
  rx_cfg.slot_cfg.left_align = false;
  rx_cfg.slot_cfg.big_endian = false;
  rx_cfg.slot_cfg.bit_order_lsb = false;
  rx_cfg.slot_cfg.skip_mask = false;
  rx_cfg.slot_cfg.total_slot = CHANNEL_COUNT;

  rx_cfg.gpio_cfg.mclk = (gpio_num_t)PIN_I2S_MCLK;
  rx_cfg.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
  rx_cfg.gpio_cfg.ws = (gpio_num_t)PIN_I2S_WS;
  rx_cfg.gpio_cfg.dout = (gpio_num_t)PIN_I2S_DOUT;
  rx_cfg.gpio_cfg.din = (gpio_num_t)PIN_I2S_DIN;
  rx_cfg.gpio_cfg.invert_flags.mclk_inv = false;
  rx_cfg.gpio_cfg.invert_flags.bclk_inv = false;
  rx_cfg.gpio_cfg.invert_flags.ws_inv = false;

  ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(i2s_rx_chan, &rx_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(i2s_rx_chan));
}

// Enable all four ES7210 microphone inputs in TDM mode.
static void initCodec()
{
  if (!es7210.begin()) {
    Serial.println("ES7210 init failed");
    while (true) {
      delay(1000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  initI2sRx();
  initCodec();

  Serial.println("Micphone example, IDF-style raw TDM read");
}

void loop()
{
  size_t bytes_read = 0;
  esp_err_t ret = i2s_channel_read(
    i2s_rx_chan,
    audio_frame,
    sizeof(audio_frame),
    &bytes_read,
    portMAX_DELAY
  );

  if (ret != ESP_OK || bytes_read != sizeof(audio_frame)) {
    Serial.printf("I2S read failed or incomplete: ret=%s, bytes_read=%u\n",
                  esp_err_to_name(ret),
                  (unsigned)bytes_read);
    delay(100);
    return;
  }

  printAudioFrame();
}
