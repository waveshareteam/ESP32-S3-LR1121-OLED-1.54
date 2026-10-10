/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

extern uint8_t rtcHour;
extern uint8_t rtcMinute;
extern bool rtcValid;
extern uint16_t batterySoc;
extern int16_t batteryCurrentMa;
extern bool batteryCharging;
extern bool batteryChargeValid;
extern bool batteryValid;

bool readRtc();
bool readBattery();
void resetSensorTimers();
void periodicSensors();
