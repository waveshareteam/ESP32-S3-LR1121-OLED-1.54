/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

struct UiGlyph {
  char ch;
  uint8_t width;
  uint8_t height;
  int8_t offsetX;
  int8_t offsetY;
  uint8_t advance;
  const uint16_t *rows;
};

static constexpr uint8_t UI_MONTSERRAT20_LINE_HEIGHT = 22;
static constexpr uint8_t UI_MONTSERRAT20_BASE_LINE = 4;

const UiGlyph *findMontserrat20Glyph(char ch);

struct UiPackedGlyph {
  uint16_t bitmapIndex;
  uint8_t width;
  uint8_t height;
  int8_t offsetX;
  int8_t offsetY;
  uint8_t advance;
};

static constexpr uint8_t UI_UNSCII8_LINE_HEIGHT = 9;

const UiPackedGlyph *findUnscii8Glyph(char ch);
bool readUnscii8GlyphPixel(const UiPackedGlyph *glyph, uint8_t col, uint8_t row);
uint16_t measureUnscii8Text(const char *text);
