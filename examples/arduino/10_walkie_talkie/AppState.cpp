/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AppState.h"

#include "Config.h"

volatile bool transmissionMode = false;

uint8_t radioModem = RADIO_MODEM_GFSK;
uint8_t radioBand = RADIO_BAND_HF;
float radioFrequencyMhz = 868.0f;
uint8_t radioLoraSpreadingFactor = 7;
float radioLoraBandwidthKhz = 125.0f;
uint8_t radioLoraCodingRate = 5;
float radioGfskBitRateKbps = 50.0f;
float radioGfskFreqDevKhz = 25.0f;
float radioGfskRxBwKhz = 117.3f;
int8_t radioTxPowerDbm = 22;
