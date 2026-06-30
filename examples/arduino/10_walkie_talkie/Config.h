/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

// Board pins, copied from the IDF BSP.
static constexpr int PIN_I2C_SDA = 13;
static constexpr int PIN_I2C_SCL = 14;

static constexpr int PIN_LR_MOSI = 45;
static constexpr int PIN_LR_MISO = 46;
static constexpr int PIN_LR_SCK = 40;
static constexpr int PIN_LR_CS = 42;
static constexpr int PIN_LR_INT = 38;
static constexpr int PIN_LR_RST = 39;
static constexpr int PIN_LR_BUSY = 41;

static constexpr int PIN_I2S_MCLK = 17;
static constexpr int PIN_I2S_BCLK = 18;
static constexpr int PIN_I2S_WS = 21;
static constexpr int PIN_I2S_DOUT = 16; // ESP32 -> ES8311
static constexpr int PIN_I2S_DIN = 15;  // ES7210 -> ESP32
static constexpr int PIN_POWER_AMP = 8;

static constexpr int PIN_KEY_BOOT = 0;
static constexpr int PIN_KEY_PWR = 4;
static constexpr int PIN_KEY_MENU = 5;
static constexpr int PIN_KEY_PTT = 6;

static constexpr bool BATTERY_CHARGE_DEBUG_LOG = false;

static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr uint8_t RTC_ADDR = 0x51;
static constexpr uint8_t BQ27220_ADDR = 0x55;
static constexpr uint8_t ES8311_ADDR = 0x18;
static constexpr uint8_t ES7210_ADDR = 0x40; // 7-bit form of ES7210 0x80

static constexpr uint16_t UI_BLACK = 0x0000;
static constexpr uint16_t UI_WHITE = 0xFFFF;
static constexpr uint16_t OLED_W = 128;
static constexpr uint16_t OLED_H = 64;

static constexpr uint32_t AUDIO_SAMPLE_RATE = 8000;
static constexpr uint8_t AUDIO_MIC_TDM_TOTAL_SLOTS = 2;
static constexpr uint32_t AUDIO_MIC_READ_TIMEOUT_MS = 100;
// Keep the customer-visible volume at 100. The ES8311 driver maps this to the
// same 0 dB route volume used by esp_codec_dev instead of writing the DAC max.
static constexpr uint8_t AUDIO_OUTPUT_VOLUME = 100;
static constexpr float AUDIO_ES8311_HW_GAIN_DB = -3.6f; // 20 * log10(3.3V / 5.0V)
static constexpr uint8_t AUDIO_MIC_GAIN_REG_VALUE = 8; // 24 dB, ES7210 gain steps are 3 dB.
static constexpr uint32_t AUDIO_SPEAKER_IDLE_MUTE_MS = 250;
static constexpr size_t AUDIO_BUFFER_SIZE = 160 * 20;
static constexpr size_t LORA_TRANSMISSION_DATA_SIZE = 240;

static constexpr uint8_t TX_START_SKIP_FRAMES = 10;
static constexpr int32_t TX_SILENCE_LEVEL = 450;
static constexpr int32_t TX_NOISE_FLOOR_INITIAL = 120;
static constexpr int32_t TX_NOISE_OPEN_MARGIN = 450;
static constexpr int32_t TX_NOISE_CLOSE_MARGIN = 250;
static constexpr uint8_t TX_VOICE_HANGOVER_FRAMES = 6;
static constexpr int32_t TX_AGC_TARGET_LEVEL = 1800;
static constexpr int32_t TX_AGC_MIN_GAIN_Q8 = 192;
static constexpr int32_t TX_AGC_MAX_GAIN_Q8 = 384;
static constexpr int32_t TX_HIGHPASS_COEFF_Q15 = 32604;
static constexpr int32_t TX_SOFT_KNEE = 12000;
static constexpr int32_t TX_PEAK_LIMIT = 20000;
static constexpr int32_t PCM_CLIP_LEVEL = 30000;
static constexpr int32_t PLAYBACK_GAIN_NUM = 3;
static constexpr int32_t PLAYBACK_GAIN_DEN = 4;
static constexpr int32_t PLAYBACK_NOISE_GATE_LEVEL = 180;
static constexpr uint8_t PLAYBACK_NOISE_GATE_HANGOVER_FRAMES = 2;
static constexpr int32_t PLAYBACK_SOFT_KNEE = 10000;
static constexpr int32_t PLAYBACK_PEAK_LIMIT = 22000;

static constexpr uint8_t RADIO_MODEM_GFSK = 0;
static constexpr uint8_t RADIO_MODEM_LORA = 1;
static constexpr uint8_t RADIO_BAND_HF = 0;
static constexpr uint8_t RADIO_BAND_LF = 1;
static constexpr uint8_t RADIO_SYNC_WORD = 0x12;
static constexpr uint8_t RADIO_PREAMBLE_LENGTH = 8;
static constexpr uint8_t RADIO_GFSK_PREAMBLE_LENGTH = 32;
static constexpr float RADIO_TCXO_VOLTAGE = 3.0f;
static constexpr uint16_t RADIO_HF_MIN_FREQ_MHZ = 862;
static constexpr uint16_t RADIO_HF_MAX_FREQ_MHZ = 923;
static constexpr uint16_t RADIO_HF_DEFAULT_FREQ_MHZ = 868;
static constexpr uint16_t RADIO_LF_MIN_FREQ_MHZ = 410;
static constexpr uint16_t RADIO_LF_MAX_FREQ_MHZ = 510;
static constexpr uint16_t RADIO_LF_DEFAULT_FREQ_MHZ = 433;
static constexpr uint8_t RADIO_HF_FREQ_STEP_MHZ = 1;
static constexpr uint8_t RADIO_LF_FREQ_STEP_MHZ = 1;
static constexpr uint16_t RADIO_MIN_FREQ_MHZ = RADIO_HF_MIN_FREQ_MHZ;
static constexpr uint16_t RADIO_MAX_FREQ_MHZ = RADIO_HF_MAX_FREQ_MHZ;

static constexpr uint8_t PAGE_MAIN = 0;
static constexpr uint8_t PAGE_SETTINGS = 1;
static constexpr uint16_t PAGE_ANIM_MS = 200;

template <typename T, size_t N>
constexpr size_t countOf(const T (&)[N]) {
  return N;
}
