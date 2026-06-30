/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>
#include "Wire.h"

#define I2C_SDA_PIN 13
#define I2C_SCL_PIN 14
static constexpr uint32_t I2C_SCAN_INTERVAL_MS = 5000;

static uint32_t last_scan_ms = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
}

void loop() {
  byte error, address;
  int nDevices = 0;

  const uint32_t now_ms = millis();
  if (now_ms - last_scan_ms < I2C_SCAN_INTERVAL_MS) {
    return;
  }
  last_scan_ms = now_ms;

  Serial.println("Scanning for I2C devices ...");
  for (address = 0x01; address < 0x7f; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("I2C device found at address 0x%02X\n", address);
      nDevices++;
    } else if (error != 2) {
      Serial.printf("Error %u at address 0x%02X\n", error, address);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found");
  }
}
