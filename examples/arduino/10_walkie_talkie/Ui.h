/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

bool initDisplay();
void initUiButtons();
void resetUiButtonsAfterWake();
void sleepDisplay();
void wakeDisplay();
void renderUi();
void refreshUiNow();
void refreshBatteryStatusNow();
void handleUiButtons();
void resetUiTimers();
void periodicUi();
void markUiDirty();
void fatalError(const char *msg, int code);
