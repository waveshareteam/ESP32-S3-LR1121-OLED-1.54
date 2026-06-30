/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

extern volatile bool transmissionMode;

extern uint8_t radioModem;
extern uint8_t radioBand;
extern float radioFrequencyMhz;
extern uint8_t radioLoraSpreadingFactor;
extern float radioLoraBandwidthKhz;
extern uint8_t radioLoraCodingRate;
extern float radioGfskBitRateKbps;
extern float radioGfskFreqDevKhz;
extern float radioGfskRxBwKhz;
extern int8_t radioTxPowerDbm;
