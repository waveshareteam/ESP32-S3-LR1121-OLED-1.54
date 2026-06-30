/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Ui.h"

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <pgmspace.h>
#include <string.h>

#include "AppState.h"
#include "ButtonState.h"
#include "Config.h"
#include "Sensors.h"
#include "Settings.h"
#include "UiFonts.h"
#include "UiIcons.h"

static Arduino_DataBus *oledBus = new Arduino_Wire(OLED_ADDR, 0x00, 0x40, &Wire);
static Arduino_G *oledPanel = new Arduino_SSD1306(oledBus, GFX_NOT_DEFINED, OLED_W, OLED_H);
static Arduino_GFX *gfx = new Arduino_Canvas_Mono(OLED_W, OLED_H, oledPanel, 0, 0, true);

static constexpr int16_t PARAM_X = 2;
static constexpr int16_t PARAM_Y = 34;
static constexpr int16_t PARAM_W = 124;
static constexpr int16_t PARAM_H = 10;
static constexpr uint16_t PARAM_SCROLL_MS_PER_PIXEL = 20;
static constexpr uint16_t ROLLER_ANIM_MS = 200;
static constexpr int16_t ROLLER_ROW_H = 10;
static constexpr int16_t ROLLER_ROW_GAP = 11;
static constexpr uint8_t MAIN_BUTTON_COUNT = 7;
static constexpr uint8_t BAND_SETTING_PAGE = 6;
static constexpr int16_t STATUS_BATTERY_FAST_X = 80;
static constexpr int16_t STATUS_BATTERY_FAST_W = OLED_W - STATUS_BATTERY_FAST_X;

static ButtonState btnBoot;
static ButtonState btnPwr;
static ButtonState btnMenu;

static bool uiDirty = true;
static uint8_t focusedButton = 0;
static uint8_t currentPage = PAGE_MAIN;
static uint8_t targetPage = PAGE_MAIN;
static uint32_t pageAnimStart = 0;
static bool pageAnimating = false;
static uint8_t settingPage = 0;
static uint8_t settingEditIndex = 0;
static uint32_t lastUiRefresh = 0;
static uint32_t lastAnimationFrame = 0;
static uint32_t lastParamScrollFrame = 0;
static uint32_t paramScrollStart = 0;
static char cachedParamText[48] = "";
static bool rollerAnimating = false;
static uint8_t rollerAnimFromIndex = 0;
static uint8_t rollerAnimToIndex = 0;
static uint32_t rollerAnimStart = 0;

static void oledWriteCommandList(const uint8_t *commands, size_t len);
static void applyOledSsd1309Tuning();

bool initDisplay() {
  bool ok = gfx->begin(400000);
  if (!ok) {
    Serial.println("[OLED] begin failed");
  } else {
    applyOledSsd1309Tuning();
  }
  gfx->fillScreen(UI_BLACK);
  gfx->flush();
  return ok;
}

void initUiButtons() {
  btnBoot.begin(PIN_KEY_BOOT);
  btnPwr.begin(PIN_KEY_PWR);
  btnMenu.begin(PIN_KEY_MENU);
}

void resetUiButtonsAfterWake() {
  initUiButtons();
}

void markUiDirty() {
  uiDirty = true;
}

static uint16_t textPixelWidth(const char *text) {
  return measureUnscii8Text(text);
}

static void drawTextClipped(int16_t x, int16_t y, int16_t clipX, int16_t clipY, int16_t clipW, int16_t clipH, const char *text, uint16_t color = UI_WHITE) {
  if (!text || text[0] == '\0') {
    return;
  }
  int16_t cursor = x;
  int16_t baseline = y + UI_UNSCII8_LINE_HEIGHT;
  for (const char *p = text; *p; p++) {
    const UiPackedGlyph *glyph = findUnscii8Glyph(*p);
    if (!glyph) {
      cursor += 8;
      continue;
    }
    int16_t glyphX = cursor + glyph->offsetX;
    int16_t glyphY = baseline - glyph->height - glyph->offsetY;
    for (uint8_t row = 0; row < glyph->height; row++) {
      int16_t py = glyphY + row;
      if (py < clipY || py >= clipY + clipH || py < 0 || py >= (int16_t)OLED_H) {
        continue;
      }
      for (uint8_t col = 0; col < glyph->width; col++) {
        int16_t px = glyphX + col;
        if (px < clipX || px >= clipX + clipW || px < 0 || px >= (int16_t)OLED_W) {
          continue;
        }
        if (readUnscii8GlyphPixel(glyph, col, row)) {
          gfx->drawPixel(px, py, color);
        }
      }
    }
    cursor += glyph->advance;
  }
}

static void drawText(int16_t x, int16_t y, const char *text, uint16_t color = UI_WHITE) {
  drawTextClipped(x, y, 0, 0, OLED_W, OLED_H, text, color);
}

static void drawCenteredTextClipped(int16_t x, int16_t y, int16_t w, int16_t h, int16_t clipX, int16_t clipY, int16_t clipW, int16_t clipH, const char *text, uint16_t color = UI_WHITE) {
  int16_t textW = textPixelWidth(text);
  int16_t textH = UI_UNSCII8_LINE_HEIGHT;
  drawTextClipped(x + (w - textW) / 2, y + (h - textH) / 2, clipX, clipY, clipW, clipH, text, color);
}

static void drawCenteredText(int16_t x, int16_t y, int16_t w, int16_t h, const char *text, uint16_t color = UI_WHITE) {
  drawCenteredTextClipped(x, y, w, h, 0, 0, OLED_W, OLED_H, text, color);
}

static void drawBitmap(int16_t x, int16_t y, const UiBitmap &bitmap, uint16_t color = UI_WHITE) {
  for (uint8_t row = 0; row < bitmap.height; row++) {
    uint16_t bits = pgm_read_word(&bitmap.rows[row]);
    for (uint8_t col = 0; col < bitmap.width; col++) {
      if (bits & (1U << (bitmap.width - 1 - col))) {
        gfx->drawPixel(x + col, y + row, color);
      }
    }
  }
}

static void statusFastSetPixel(uint8_t *page, int16_t x, int16_t y) {
  if (x < STATUS_BATTERY_FAST_X || x >= (STATUS_BATTERY_FAST_X + STATUS_BATTERY_FAST_W) || y < 0 || y >= 8) {
    return;
  }
  page[x - STATUS_BATTERY_FAST_X] |= (uint8_t)(1U << y);
}

static void drawStatusFastText(uint8_t *page, int16_t x, int16_t y, const char *text) {
  if (!text || text[0] == '\0') {
    return;
  }
  int16_t cursor = x;
  int16_t baseline = y + UI_UNSCII8_LINE_HEIGHT;
  for (const char *p = text; *p; p++) {
    const UiPackedGlyph *glyph = findUnscii8Glyph(*p);
    if (!glyph) {
      cursor += 8;
      continue;
    }
    int16_t glyphX = cursor + glyph->offsetX;
    int16_t glyphY = baseline - glyph->height - glyph->offsetY;
    for (uint8_t row = 0; row < glyph->height; row++) {
      for (uint8_t col = 0; col < glyph->width; col++) {
        if (readUnscii8GlyphPixel(glyph, col, row)) {
          statusFastSetPixel(page, glyphX + col, glyphY + row);
        }
      }
    }
    cursor += glyph->advance;
  }
}

static void drawStatusFastBitmap(uint8_t *page, int16_t x, int16_t y, const UiBitmap &bitmap) {
  for (uint8_t row = 0; row < bitmap.height; row++) {
    uint16_t bits = pgm_read_word(&bitmap.rows[row]);
    for (uint8_t col = 0; col < bitmap.width; col++) {
      if (bits & (1U << (bitmap.width - 1 - col))) {
        statusFastSetPixel(page, x + col, y + row);
      }
    }
  }
}

static void oledWriteCommandList(const uint8_t *commands, size_t len) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  for (size_t i = 0; i < len; i++) {
    Wire.write(commands[i]);
  }
  Wire.endTransmission();
}

static void applyOledSsd1309Tuning() {
  const uint8_t commands[] = {
      0x8D, 0x14,  // Charge pump enable
      0x81, 0xCF,  // Contrast from the IDF SSD1309 panel init
      0xD9, 0xF1,  // Pre-charge period
      0xDB, 0x40,  // VCOMH deselect level
      0xA4,        // Resume RAM content display
      0xA6,        // Normal display
      0xAF,        // Display on
  };
  oledWriteCommandList(commands, sizeof(commands));
}

static void oledWriteDataList(const uint8_t *data, size_t len) {
  static constexpr size_t CHUNK = 24;
  for (size_t offset = 0; offset < len; offset += CHUNK) {
    size_t chunkLen = len - offset;
    if (chunkLen > CHUNK) {
      chunkLen = CHUNK;
    }
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40);
    for (size_t i = 0; i < chunkLen; i++) {
      Wire.write(data[offset + i]);
    }
    Wire.endTransmission();
  }
}

void sleepDisplay() {
  gfx->fillScreen(UI_BLACK);
  gfx->flush();
  const uint8_t commands[] = {0xAE};
  oledWriteCommandList(commands, sizeof(commands));
}

void wakeDisplay() {
  applyOledSsd1309Tuning();
  resetUiTimers();
  refreshUiNow();
}

static void flushBatteryStatusFast() {
  uint8_t page[STATUS_BATTERY_FAST_W] = {};
  char text[12];
  if (batteryValid) {
    snprintf(text, sizeof(text), "%u%%", batterySoc);
  } else {
    snprintf(text, sizeof(text), "--%%");
  }

  int16_t batTextW = textPixelWidth(text);
  drawStatusFastText(page, 114 - batTextW, 0, text);

  const UiBitmap *icon = &kIconBattery4;
  if (batteryValid && batteryCharging) {
    icon = &kIconCharge;
  } else if (batteryValid) {
    icon = batterySoc > 75 ? &kIconBattery4 : batterySoc > 50 ? &kIconBattery3 : batterySoc > 25 ? &kIconBattery2 : &kIconBattery1;
  }
  drawStatusFastBitmap(page, 116, 0, *icon);

  const uint8_t commands[] = {
      0x20, 0x00,
      0x21, (uint8_t)STATUS_BATTERY_FAST_X, (uint8_t)(OLED_W - 1),
      0x22, 0x00, 0x00,
  };
  oledWriteCommandList(commands, sizeof(commands));
  oledWriteDataList(page, sizeof(page));
  const uint8_t restoreCommands[] = {
      0x21, 0x00, (uint8_t)(OLED_W - 1),
      0x22, 0x00, (uint8_t)(OLED_H / 8 - 1),
  };
  oledWriteCommandList(restoreCommands, sizeof(restoreCommands));
}

static void drawMontserrat20Text(int16_t x, int16_t y, const char *text, uint16_t color = UI_WHITE) {
  if (!text) {
    return;
  }
  int16_t cursor = x;
  int16_t baseline = y + UI_MONTSERRAT20_LINE_HEIGHT - UI_MONTSERRAT20_BASE_LINE;
  for (const char *p = text; *p; p++) {
    const UiGlyph *glyph = findMontserrat20Glyph(*p);
    if (!glyph) {
      cursor += 6;
      continue;
    }
    int16_t glyphX = cursor + glyph->offsetX;
    int16_t glyphY = baseline - glyph->height - glyph->offsetY;
    for (uint8_t row = 0; row < glyph->height; row++) {
      uint16_t bits = pgm_read_word(&glyph->rows[row]);
      for (uint8_t col = 0; col < glyph->width; col++) {
        if (bits & (1U << (glyph->width - 1 - col))) {
          gfx->drawPixel(glyphX + col, glyphY + row, color);
        }
      }
    }
    cursor += glyph->advance;
  }
}

static bool syncParamText(char *params, size_t paramsSize) {
  formatParams(params, paramsSize);
  if (strcmp(params, cachedParamText) != 0) {
    snprintf(cachedParamText, sizeof(cachedParamText), "%s", params);
    paramScrollStart = millis();
    lastParamScrollFrame = millis();
  }
  return textPixelWidth(params) > PARAM_W;
}

static bool paramTextNeedsScroll() {
  char params[48];
  return syncParamText(params, sizeof(params));
}

static void drawParamText(int16_t y) {
  char params[48];
  bool scrolling = syncParamText(params, sizeof(params));
  int16_t textW = textPixelWidth(params);
  if (!scrolling) {
    drawTextClipped(PARAM_X, y + PARAM_Y, PARAM_X, y + PARAM_Y, PARAM_W, PARAM_H, params);
    return;
  }

  const int16_t gap = 16;
  const int16_t loopW = textW + gap;
  int16_t offset = ((millis() - paramScrollStart) / PARAM_SCROLL_MS_PER_PIXEL) % loopW;
  for (int16_t x = PARAM_X - offset; x < PARAM_X + PARAM_W; x += loopW) {
    drawTextClipped(x, y + PARAM_Y, PARAM_X, y + PARAM_Y, PARAM_W, PARAM_H, params);
  }
}

static void drawIconButton(int16_t x, int16_t y, const UiBitmap &icon, bool focused) {
  if (focused) {
    gfx->drawRect(x - 1, y - 1, 21, 18, UI_WHITE);
  }
  gfx->drawRect(x, y, 19, 16, UI_WHITE);
  drawBitmap(x + (19 - icon.width) / 2, y + (16 - icon.height) / 2, icon);
}

static void drawModeBandButton(int16_t y) {
  char label[8];
  snprintf(label, sizeof(label), "%c-%s", radioModem == RADIO_MODEM_GFSK ? 'G' : 'L', radioBandLabel());
  if (focusedButton == BAND_SETTING_PAGE) {
    gfx->drawRect(95, y + 12, 32, 12, UI_WHITE);
  }
  drawTextClipped(96, y + 13, 96, y + 13, 30, 10, label);
}

static void drawBatteryIcon() {
  const UiBitmap *icon = &kIconBattery4;
  if (batteryValid && batteryCharging) {
    icon = &kIconCharge;
  } else if (batteryValid) {
    icon = batterySoc > 75 ? &kIconBattery4 : batterySoc > 50 ? &kIconBattery3 : batterySoc > 25 ? &kIconBattery2 : &kIconBattery1;
  }
  drawBitmap(116, 0, *icon);
}

static void drawStatusBar() {
  gfx->fillRect(0, 0, OLED_W, 10, UI_BLACK);
  char text[12];
  if (rtcValid) {
    snprintf(text, sizeof(text), "%02u:%02u", rtcHour, rtcMinute);
  } else {
    snprintf(text, sizeof(text), "--:--");
  }
  drawText(2, 0, text);

  if (batteryValid) {
    snprintf(text, sizeof(text), "%u%%", batterySoc);
  } else {
    snprintf(text, sizeof(text), "--%%");
  }
  int16_t batTextW = textPixelWidth(text);
  drawText(114 - batTextW, 0, text);
  drawBatteryIcon();
}

static void drawMainPage(int16_t y) {
  if (y <= -64 || y >= 64) {
    return;
  }

  const bool tx = transmissionMode;
  if (tx) {
    gfx->fillRect(2, y + 14, 27, 17, UI_WHITE);
    gfx->drawRect(2, y + 14, 27, 17, UI_WHITE);
    drawCenteredText(2, y + 14, 27, 17, "TX", UI_BLACK);
  } else {
    gfx->drawRect(2, y + 14, 27, 17, UI_WHITE);
    drawCenteredText(2, y + 14, 27, 17, "RX");
  }

  char freq[12];
  snprintf(freq, sizeof(freq), "%03d.0", (int)(radioFrequencyMhz + 0.5f));
  drawMontserrat20Text(35, y + 11, freq);
  drawText(101, y + 24, "MHz");
  drawModeBandButton(y);
  drawParamText(y);

  drawIconButton(2, y + 46, radioModem == RADIO_MODEM_GFSK ? kIconMdG : kIconMdL, focusedButton == 0);
  drawIconButton(23, y + 46, kIconHz, focusedButton == 1);
  drawIconButton(44, y + 46, radioModem == RADIO_MODEM_GFSK ? kIconBr : kIconSf, focusedButton == 2);
  drawIconButton(65, y + 46, radioModem == RADIO_MODEM_GFSK ? kIconDv : kIconBw, focusedButton == 3);
  drawIconButton(86, y + 46, radioModem == RADIO_MODEM_GFSK ? kIconRx : kIconCr, focusedButton == 4);
  drawIconButton(107, y + 46, kIconPw, focusedButton == 5);
}

static void startRollerMove(uint8_t newIndex) {
  if (newIndex == settingEditIndex) {
    return;
  }
  rollerAnimating = true;
  rollerAnimFromIndex = settingEditIndex;
  rollerAnimToIndex = newIndex;
  rollerAnimStart = millis();
  settingEditIndex = newIndex;
  markUiDirty();
}

static void drawRollerContent(int16_t x, int16_t y, int16_t w, uint8_t originIndex, int16_t offsetY, uint16_t color, int16_t clipX, int16_t clipY, int16_t clipW, int16_t clipH) {
  int16_t selectedY = y + 13;
  uint8_t count = optionCount(settingPage);
  char label[20];
  for (int8_t rel = -3; rel <= 3; rel++) {
    int16_t index = (int16_t)originIndex + rel;
    if (index < 0 || index >= count) {
      continue;
    }
    settingOptionText(settingPage, (uint8_t)index, label, sizeof(label));
    int16_t rowY = selectedY + rel * ROLLER_ROW_GAP + offsetY;
    drawCenteredTextClipped(x, rowY, w, ROLLER_ROW_H, clipX, clipY, clipW, clipH, label, color);
  }
}

static void drawSettingsPage(int16_t y) {
  if (y <= -64 || y >= 64) {
    return;
  }
  gfx->fillRect(0, y, OLED_W, OLED_H, UI_BLACK);
  drawText(2, y + 11, settingTitle(settingPage));

  const int16_t rx = 17;
  const int16_t ry = y + 26;
  const int16_t rw = 94;
  const int16_t rh = 36;
  gfx->drawRect(rx, ry, rw, rh, UI_WHITE);

  uint8_t originIndex = settingEditIndex;
  int16_t textOffset = 0;
  if (rollerAnimating) {
    uint32_t elapsed = millis() - rollerAnimStart;
    if (elapsed >= ROLLER_ANIM_MS) {
      rollerAnimating = false;
    } else {
      originIndex = rollerAnimFromIndex;
      int16_t step = (int16_t)((elapsed * ROLLER_ROW_GAP) / ROLLER_ANIM_MS);
      textOffset = rollerAnimToIndex > rollerAnimFromIndex ? -step : step;
    }
  }

  const int16_t innerX = rx + 1;
  const int16_t innerY = ry + 1;
  const int16_t innerW = rw - 2;
  const int16_t innerH = rh - 2;
  const int16_t selectedY = ry + 13;

  drawRollerContent(innerX, ry, innerW, originIndex, textOffset, UI_WHITE, innerX, innerY, innerW, innerH);
  gfx->fillRect(innerX, selectedY, innerW, ROLLER_ROW_H, UI_WHITE);
  drawRollerContent(innerX, ry, innerW, originIndex, textOffset, UI_BLACK, innerX, selectedY, innerW, ROLLER_ROW_H);
}

static void startPageTransition(uint8_t page) {
  if (page == currentPage && !pageAnimating) {
    return;
  }
  targetPage = page;
  pageAnimStart = millis();
  pageAnimating = true;
  markUiDirty();
}

static void pageOffsets(int16_t *mainY, int16_t *settingsY) {
  if (!pageAnimating) {
    *mainY = currentPage == PAGE_MAIN ? 0 : -64;
    *settingsY = currentPage == PAGE_SETTINGS ? 0 : 64;
    return;
  }

  uint32_t elapsed = millis() - pageAnimStart;
  if (elapsed >= PAGE_ANIM_MS) {
    pageAnimating = false;
    currentPage = targetPage;
    *mainY = currentPage == PAGE_MAIN ? 0 : -64;
    *settingsY = currentPage == PAGE_SETTINGS ? 0 : 64;
    return;
  }

  int16_t step = (int16_t)((elapsed * 64) / PAGE_ANIM_MS);
  if (targetPage == PAGE_SETTINGS) {
    *mainY = -step;
    *settingsY = 64 - step;
  } else {
    *mainY = -64 + step;
    *settingsY = step;
  }
}

void renderUi() {
  int16_t mainY = 0;
  int16_t settingsY = 64;
  pageOffsets(&mainY, &settingsY);

  gfx->fillScreen(UI_BLACK);
  drawMainPage(mainY);
  drawSettingsPage(settingsY);
  drawStatusBar();
  gfx->flush();
}

void refreshUiNow() {
  renderUi();
  lastUiRefresh = millis();
  uiDirty = false;
}

void refreshBatteryStatusNow() {
  drawStatusBar();
  flushBatteryStatusFast();
  lastUiRefresh = millis();
}

static void openSettings(uint8_t page) {
  settingPage = page;
  settingEditIndex = currentSettingIndex(page);
  rollerAnimating = false;
  startPageTransition(PAGE_SETTINGS);
}

void handleUiButtons() {
  btnBoot.update();
  btnPwr.update();
  btnMenu.update();

  if (pageAnimating || rollerAnimating) {
    return;
  }

  if (currentPage == PAGE_MAIN) {
    if (btnBoot.takePressOrRepeat()) {
      focusedButton = (focusedButton + MAIN_BUTTON_COUNT - 1) % MAIN_BUTTON_COUNT;
      markUiDirty();
    }
    if (btnPwr.takePressOrRepeat()) {
      focusedButton = (focusedButton + 1) % MAIN_BUTTON_COUNT;
      markUiDirty();
    }
    if (btnMenu.takePressed()) {
      openSettings(focusedButton);
    }
  } else {
    uint8_t count = optionCount(settingPage);
    if (btnBoot.takePressOrRepeat()) {
      if (settingEditIndex > 0) {
        startRollerMove(settingEditIndex - 1);
      }
    }
    if (btnPwr.takePressOrRepeat()) {
      if (settingEditIndex + 1 < count) {
        startRollerMove(settingEditIndex + 1);
      }
    }
    if (btnMenu.takePressed()) {
      applySettingSelection(settingPage, settingEditIndex);
      startPageTransition(PAGE_MAIN);
    }
  }
}

void resetUiTimers() {
  lastUiRefresh = millis();
  lastAnimationFrame = millis();
  lastParamScrollFrame = millis();
  paramScrollStart = millis();
}

void periodicUi() {
  uint32_t now = millis();
  bool animFrame = (pageAnimating || rollerAnimating) && (now - lastAnimationFrame >= 20);
  bool scrollFrame = (currentPage == PAGE_MAIN || pageAnimating) && paramTextNeedsScroll() && (now - lastParamScrollFrame >= PARAM_SCROLL_MS_PER_PIXEL);
  bool refreshFrame = now - lastUiRefresh >= 300;
  if (uiDirty || animFrame || scrollFrame || refreshFrame) {
    renderUi();
    if (animFrame) {
      lastAnimationFrame = now;
    }
    if (scrollFrame) {
      lastParamScrollFrame = now;
    }
    if (refreshFrame) {
      lastUiRefresh = now;
    }
    uiDirty = false;
  }
}

void fatalError(const char *msg, int code) {
  Serial.printf("[Fatal] %s: %d\n", msg, code);
  gfx->fillScreen(UI_BLACK);
  drawText(0, 12, "FATAL");
  drawText(0, 24, msg);
  char line[20];
  snprintf(line, sizeof(line), "%d", code);
  drawText(0, 36, line);
  gfx->flush();
  while (true) {
    delay(1000);
  }
}
