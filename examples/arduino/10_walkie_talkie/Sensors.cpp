/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Sensors.h"

#include <Wire.h>
#include <algorithm>

#include "BatteryGauge.h"
#include "Config.h"
#include "Ui.h"

uint8_t rtcHour = 0;
uint8_t rtcMinute = 0;
bool rtcValid = false;
uint16_t batterySoc = 0;
int16_t batteryCurrentMa = 0;
bool batteryCharging = false;
bool batteryChargeValid = false;
bool batteryValid = false;

static uint32_t lastRtcRead = 0;
static uint32_t lastBatteryRead = 0;
static uint32_t lastBatteryAttempt = 0;
static bool batteryReadAttempted = false;
static uint16_t batteryVoltageMv = 0;
static uint16_t batteryRemainingMah = 0;
static uint16_t batteryFullMah = 0;
static bool batteryFullFlag = false;

static constexpr uint16_t RTC_READ_MS = 1000;
// BQ27220 datasheet SLUSCB7A, section 7.3.1.3: do not issue a standard
// command more than twice per second. Read SOC and current together once per
// second, including startup/wake calls, without a second faster polling path.
static constexpr uint16_t BATTERY_READ_MS = 1000;
static constexpr uint8_t BQ_REG_VOLTAGE = 0x08;
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

static void logChargeSample(bool changed) {
  if (!BATTERY_CHARGE_DEBUG_LOG) {
    return;
  }
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  if (!changed && now - lastLog < 1000) {
    return;
  }
  lastLog = now;
  WALKIE_LOG_PRINTF("[Battery] current=%dmA current_ok=%u soc=%u%% soc_ok=%u charging=%u "
                "voltage=%umV remaining=%umAh fcc=%umAh full=%u details_ok=%u\n",
                batteryCurrentMa, (unsigned)batteryChargeValid,
                batterySoc, (unsigned)batteryValid, (unsigned)batteryCharging,
                batteryVoltageMv, batteryRemainingMah, batteryFullMah,
                (unsigned)batteryFullFlag, (unsigned)batteryChargeValid);
}

static void readBatteryChargeStatus() {
  bool wasValid = batteryChargeValid;
  bool wasCharging = batteryCharging;
  // One sequential read covers voltage, flags, current, RM and FCC (0x08..0x13).
  // Diagnostics add no extra standard-command polls to the normal 1 Hz loop.
  uint8_t data[12];
  batteryChargeValid = readBatteryGaugeRegisters(BQ_REG_VOLTAGE, data, sizeof(data));
  if (!batteryChargeValid) {
    batteryCharging = false;
    logChargeSample(wasValid || wasCharging);
    return;
  }

  batteryVoltageMv = batteryGaugeU16(data);
  batteryFullFlag = (batteryGaugeU16(data + 2) & 0x0200) != 0;
  batteryCurrentMa = (int16_t)batteryGaugeU16(data + 4);
  batteryRemainingMah = batteryGaugeU16(data + 8);
  batteryFullMah = batteryGaugeU16(data + 10);
  // Zero current is not evidence of charging. The gauge reports signed net
  // battery current, independently of whether the SOC register can be read.
  batteryCharging = batteryCurrentMa > 0;
  logChargeSample(!wasValid || wasCharging != batteryCharging);
}

bool readBattery() {
  // Configuration and polling share a gauge. Resume after its settle deadline;
  // RTC, OLED, buttons and radio continue while the gauge is initializing.
  if (batteryGaugeInitInProgress()) {
    return batteryValid;
  }
  uint32_t now = millis();
  if (batteryReadAttempted && now - lastBatteryAttempt < BATTERY_READ_MS) {
    return batteryValid;
  }
  // Limit attempts as well as successful reads; bus failures must not trigger
  // a burst of retries that can reset the gauge's watchdog.
  batteryReadAttempted = true;
  lastBatteryAttempt = now;
  uint8_t data[2];
  batteryValid = readBatteryGaugeRegisters(BQ_REG_STATE_OF_CHARGE, data, sizeof(data));
  if (batteryValid) {
    batterySoc = std::min<uint16_t>(batteryGaugeU16(data), 100);
  }
  // A failed SOC read must not skip the independent charging measurement.
  readBatteryChargeStatus();
  return batteryValid;
}

void resetSensorTimers() {
  lastRtcRead = millis();
  lastBatteryRead = millis();
}

void periodicSensors() {
  uint32_t now = millis();
  if (now - lastRtcRead >= RTC_READ_MS) {
    readRtc();
    lastRtcRead = now;
    markUiDirty();
  }

  if (now - lastBatteryRead >= BATTERY_READ_MS) {
    bool wasValid = batteryValid;
    bool wasChargeValid = batteryChargeValid;
    bool wasCharging = batteryCharging;
    uint16_t oldSoc = batterySoc;
    readBattery();
    lastBatteryRead = now;
    if (wasValid != batteryValid || wasChargeValid != batteryChargeValid ||
        wasCharging != batteryCharging || oldSoc != batterySoc) {
      refreshBatteryStatusNow();
    }
  }
}
