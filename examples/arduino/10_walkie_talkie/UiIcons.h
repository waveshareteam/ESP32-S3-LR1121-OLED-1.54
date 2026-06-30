/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

struct UiBitmap {
  uint8_t width;
  uint8_t height;
  const uint16_t *rows;
};

extern const UiBitmap kIconMdG;
extern const UiBitmap kIconMdL;
extern const UiBitmap kIconHz;
extern const UiBitmap kIconBr;
extern const UiBitmap kIconDv;
extern const UiBitmap kIconRx;
extern const UiBitmap kIconSf;
extern const UiBitmap kIconBw;
extern const UiBitmap kIconCr;
extern const UiBitmap kIconPw;
extern const UiBitmap kIconBattery1;
extern const UiBitmap kIconBattery2;
extern const UiBitmap kIconBattery3;
extern const UiBitmap kIconBattery4;
extern const UiBitmap kIconCharge;
