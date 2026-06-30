/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

struct ButtonState {
  static constexpr uint32_t DEBOUNCE_MS = 25;
  static constexpr uint32_t REPEAT_DELAY_MS = 450;
  static constexpr uint32_t REPEAT_INTERVAL_MS = 120;

  uint8_t pin = 0;
  bool stable = true;
  bool lastRead = true;
  bool pressedEvent = false;
  bool repeatEvent = false;
  uint32_t changedAt = 0;
  uint32_t pressedAt = 0;
  uint32_t lastRepeatAt = 0;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    stable = digitalRead(pin);
    lastRead = stable;
    pressedEvent = false;
    repeatEvent = false;
    changedAt = millis();
    pressedAt = 0;
    lastRepeatAt = 0;
  }

  void update() {
    bool nowRead = digitalRead(pin);
    uint32_t now = millis();
    if (nowRead != lastRead) {
      lastRead = nowRead;
      changedAt = now;
    }
    if ((now - changedAt) >= DEBOUNCE_MS && nowRead != stable) {
      stable = nowRead;
      if (stable == LOW) {
        pressedEvent = true;
        pressedAt = now;
        lastRepeatAt = now;
      } else {
        repeatEvent = false;
        pressedAt = 0;
        lastRepeatAt = 0;
      }
    }
    if (stable == LOW && pressedAt != 0 &&
        (now - pressedAt) >= REPEAT_DELAY_MS &&
        (now - lastRepeatAt) >= REPEAT_INTERVAL_MS) {
      repeatEvent = true;
      lastRepeatAt = now;
    }
  }

  bool takePressed() {
    bool ret = pressedEvent;
    pressedEvent = false;
    repeatEvent = false;
    return ret;
  }

  bool takePressOrRepeat() {
    bool ret = pressedEvent || repeatEvent;
    pressedEvent = false;
    repeatEvent = false;
    return ret;
  }
};
