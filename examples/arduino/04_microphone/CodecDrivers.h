/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

class Es8311Codec {
public:
  bool begin(uint32_t sampleRate, uint8_t volume);
  bool setVolume(uint8_t volume);

private:
  bool setSampleRate(uint32_t sampleRate);
  bool setBitsPerSample(uint8_t bits);
  bool probe(uint8_t addr);
  bool updateReg(uint8_t reg, uint8_t mask, uint8_t value);
  bool writeReg(uint8_t reg, uint8_t value);
  uint8_t readReg(uint8_t reg);
};

class Es7210Adc {
public:
  bool begin();

private:
  bool probe(uint8_t addr);
  bool selectAllMics();
  bool setAllMicGain(uint8_t gain);
  bool setBits(uint8_t bits);
  bool configI2SFormat();
  bool start();
  bool updateReg(uint8_t reg, uint8_t mask, uint8_t value);
  bool writeReg(uint8_t reg, uint8_t value);
  uint8_t readReg(uint8_t reg);

  uint8_t offReg_ = 0x20;
};
