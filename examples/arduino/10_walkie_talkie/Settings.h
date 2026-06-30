/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

void loadRadioSettings();
uint8_t optionCount(uint8_t page);
uint8_t currentSettingIndex(uint8_t page);
const char *settingTitle(uint8_t page);
void settingOptionText(uint8_t page, uint8_t index, char *out, size_t outSize);
void formatParams(char *out, size_t outSize);
const char *radioBandLabel();
void applySettingSelection(uint8_t page, uint8_t index);
