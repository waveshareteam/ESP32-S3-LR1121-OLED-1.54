/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

// Start once in setup, then service from loop. All I2C stays on the main task.
void startBatteryGaugeInit();
void serviceBatteryGaugeInit();
bool batteryGaugeInitInProgress();
bool readBatteryGaugeRegisters(uint8_t reg, uint8_t *data, uint8_t size);

inline uint16_t batteryGaugeU16(const uint8_t *data) {
  return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}
