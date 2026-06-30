/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RADIO_DEVICE_H
#define RADIO_DEVICE_H

#include "EspHal.h"

// Global radio objects are shared by setup, IRQ processing and UI callbacks.
extern EspHal *hal;
extern LR1121 radio;

#endif
