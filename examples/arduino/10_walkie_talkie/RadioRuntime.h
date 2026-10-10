/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

void initRadioSpi();
void requestRadioReconfigure();
int16_t applyRadioConfig(bool startReceive);
bool startRadioConfigTask();
void handleRadioReconfigure();
void handlePtt();
void handleRadioTransmission();
void handleRadioIrq();
void logRadioDiagnostics();
bool radioRuntimeIsBusy();
bool isRadioIrqAsserted();
void clearRadioIrqPending();
void setRadioIrqPending();
void detachRadioIrqForSleep();
void restoreRadioIrqAction();
int16_t startRadioReceiveForSleep();
void resetRadioRuntimeAfterSleep(bool pttPressed, bool irqPending);
