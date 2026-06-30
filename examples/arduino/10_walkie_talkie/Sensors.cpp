/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Sensors.h"

#include <Wire.h>
#include <algorithm>

#include "Config.h"
#include "Ui.h"

uint8_t rtcHour = 0;
uint8_t rtcMinute = 0;
bool rtcValid = false;
uint16_t batterySoc = 0;
int16_t batteryCurrentMa = 0;
bool batteryCharging = false;
bool batteryValid = false;

static uint32_t lastRtcRead = 0;
static uint32_t lastBatteryRead = 0;
static uint32_t lastBatteryChargeRead = 0;

static constexpr uint16_t RTC_READ_MS = 1000;
static constexpr uint16_t BATTERY_SOC_READ_MS = 1000;
static constexpr uint16_t BATTERY_CHARGE_READ_MS = 300;
static constexpr uint8_t BQ_REG_CURRENT = 0x0C;
static constexpr uint8_t BQ_REG_STATE_OF_CHARGE = 0x2C;

static uint8_t bcdToDec(uint8_t value) {
  return ((value >> 4) * 10) + (value & 0x0F);
}

bool readRtc() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0x04);
  if (Wire.endTransmission(false) != 0) {
    rtcValid = false;
    return false;
  }
  if (Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)3) != 3) {
    rtcValid = false;
    return false;
  }
  uint8_t sec = Wire.read() & 0x7F;
  uint8_t minReg = Wire.read() & 0x7F;
  uint8_t hourReg = Wire.read() & 0x3F;
  (void)sec;
  rtcMinute = bcdToDec(minReg);
  rtcHour = bcdToDec(hourReg);
  rtcValid = rtcHour < 24 && rtcMinute < 60;
  return rtcValid;
}

static bool readBqU16(uint8_t reg, uint16_t *value) {
  Wire.beginTransmission(BQ27220_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom((uint8_t)BQ27220_ADDR, (uint8_t)2) != 2) {
    return false;
  }
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  *value = ((uint16_t)hi << 8) | lo;
  return true;
}

static bool readBqI16(uint8_t reg, int16_t *value) {
  uint16_t raw = 0;
  if (!readBqU16(reg, &raw)) {
    return false;
  }
  *value = (int16_t)raw;
  return true;
}

static void logChargeSample(int16_t currentMa, bool changed) {
  if (!BATTERY_CHARGE_DEBUG_LOG) {
    return;
  }
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (!changed && now - lastLog < 500) {
    return;
  }
  lastLog = now;
  Serial.printf("[Battery] current=%d charging=%u\n", currentMa, (unsigned)batteryCharging);
}

bool readBatteryChargeStatus() {
  int16_t current = 0;
  if (!readBqI16(BQ_REG_CURRENT, &current)) {
    return false;
  }

  bool changed = batteryCharging != (current > 0);
  batteryCurrentMa = current;
  batteryCharging = current >= 0;
  logChargeSample(current, changed);
  return true;
}

bool readBattery() {
  uint16_t soc = 0;
  if (!readBqU16(BQ_REG_STATE_OF_CHARGE, &soc)) {
    batteryValid = false;
    return false;
  }
  batterySoc = std::min<uint16_t>(soc, 100);
  batteryValid = true;
  readBatteryChargeStatus();
  return true;
}

void resetSensorTimers() {
  lastRtcRead = millis();
  lastBatteryRead = millis();
  lastBatteryChargeRead = millis();
}

void periodicSensors() {
  uint32_t now = millis();
  if (now - lastRtcRead >= RTC_READ_MS) {
    readRtc();
    lastRtcRead = now;
    markUiDirty();
  }

  bool batterySocDue = now - lastBatteryRead >= BATTERY_SOC_READ_MS;
  if (batterySocDue) {
    bool wasValid = batteryValid;
    bool wasCharging = batteryCharging;
    uint16_t oldSoc = batterySoc;
    readBattery();
    lastBatteryRead = now;
    lastBatteryChargeRead = now;
    if (wasValid != batteryValid || wasCharging != batteryCharging || oldSoc != batterySoc) {
      refreshBatteryStatusNow();
    }
    return;
  }

  if (now - lastBatteryChargeRead >= BATTERY_CHARGE_READ_MS) {
    bool wasValid = batteryValid;
    bool wasCharging = batteryCharging;
    if (readBatteryChargeStatus() && (wasValid != batteryValid || wasCharging != batteryCharging)) {
      refreshBatteryStatusNow();
    }
    lastBatteryChargeRead = now;
  }
}
