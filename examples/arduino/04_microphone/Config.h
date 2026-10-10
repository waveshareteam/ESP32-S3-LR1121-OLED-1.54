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

static constexpr int PIN_I2S_MCLK = 17;
static constexpr int PIN_I2S_BCLK = 18;
static constexpr int PIN_I2S_WS = 21;
static constexpr int PIN_I2S_DOUT = 16; // ESP32 -> ES8311
static constexpr int PIN_I2S_DIN = 15;  // ES7210 -> ESP32

static constexpr uint8_t ES8311_ADDR = 0x18;
static constexpr uint8_t ES7210_ADDR = 0x40; // 7-bit form of ES7210 0x80

static constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;
static constexpr uint8_t AUDIO_MIC_TDM_TOTAL_SLOTS = 4;
static constexpr uint8_t AUDIO_OUTPUT_VOLUME = 100;
static constexpr float AUDIO_ES8311_HW_GAIN_DB = -3.6f; // 20 * log10(3.3V / 5.0V)
static constexpr uint8_t AUDIO_MIC_GAIN_REG_VALUE = 10; // 30 dB, ES7210 gain steps are 3 dB.
