/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CodecDrivers.h"

#include <Wire.h>
#include <algorithm>

#include "Config.h"

bool Es8311Codec::begin(uint32_t sampleRate, uint8_t volume) {
  if (!probe(ES8311_ADDR)) {
    WALKIE_LOG_PRINTLN("[ES8311] not found");
    return false;
  }

  bool ok = true;
  ok &= writeReg(0x00, 0x1F);
  delay(20);
  ok &= writeReg(0x00, 0x00);
  ok &= writeReg(0x00, 0x80);
  ok &= writeReg(0x01, 0x3F);
  ok &= updateReg(0x06, 0x20, 0x00);
  ok &= setSampleRate(sampleRate);
  ok &= setBitsPerSample(16);
  ok &= writeReg(0x0D, 0x01);
  ok &= writeReg(0x0E, 0x02);
  ok &= writeReg(0x12, 0x00);
  ok &= writeReg(0x13, 0x10);
  ok &= writeReg(0x14, 0x1A);
  ok &= writeReg(0x15, 0x40);
  ok &= writeReg(0x1B, 0x0A);
  ok &= writeReg(0x1C, 0x6A);
  ok &= writeReg(0x37, 0x08);
  ok &= writeReg(0x44, 0x58);
  ok &= setVolume(volume);
  return ok;
}

bool Es8311Codec::setVolume(uint8_t volume) {
  volume = std::min<uint8_t>(volume, 100);
  if (volume == 0) {
    return writeReg(0x32, 0x00);
  }

  float routeDb = -50.0f + (float)volume * 0.5f;
  float dacDb = routeDb - AUDIO_ES8311_HW_GAIN_DB;
  if (dacDb <= -95.5f) {
    return writeReg(0x32, 0x00);
  }
  if (dacDb >= 32.0f) {
    return writeReg(0x32, 0xFF);
  }

  uint8_t reg32 = (uint8_t)((dacDb + 95.5f) * 2.0f + 0.5f);
  return writeReg(0x32, reg32);
}

bool Es8311Codec::sleep() {
  bool ok = true;
  ok &= writeReg(0x32, 0x00);
  ok &= writeReg(0x17, 0x00);
  ok &= writeReg(0x0E, 0xFF);
  ok &= writeReg(0x12, 0x02);
  ok &= writeReg(0x14, 0x00);
  ok &= writeReg(0x0D, 0xFA);
  ok &= writeReg(0x15, 0x00);
  ok &= writeReg(0x02, 0x10);
  ok &= writeReg(0x00, 0x00);
  ok &= writeReg(0x00, 0x1F);
  ok &= writeReg(0x01, 0x30);
  ok &= writeReg(0x01, 0x00);
  ok &= writeReg(0x45, 0x00);
  ok &= writeReg(0x0D, 0xFC);
  ok &= writeReg(0x02, 0x00);
  return ok;
}

bool Es8311Codec::resume(uint32_t sampleRate, uint8_t volume) {
  return begin(sampleRate, volume);
}

bool Es8311Codec::setSampleRate(uint32_t sampleRate) {
  if (sampleRate != 8000) {
    return false;
  }

  const uint8_t preDiv = 1;
  const uint8_t preMulti = 0;
  const uint8_t adcDiv = 1;
  const uint8_t dacDiv = 1;
  const uint8_t fsMode = 0;
  const uint8_t lrckH = 0;
  const uint8_t lrckL = 0xFF;
  const uint8_t bclkDiv = 4;
  const uint8_t adcOsr = 0x10;
  const uint8_t dacOsr = 0x20;

  bool ok = true;
  uint8_t reg = readReg(0x02);
  reg &= 0x07;
  reg |= (preDiv - 1) << 5;
  reg |= preMulti << 3;
  ok &= writeReg(0x02, reg);
  ok &= writeReg(0x03, (fsMode << 6) | adcOsr);
  ok &= writeReg(0x04, dacOsr);
  ok &= writeReg(0x05, ((adcDiv - 1) << 4) | (dacDiv - 1));
  reg = readReg(0x06);
  reg &= 0xE0;
  reg |= (bclkDiv - 1);
  ok &= writeReg(0x06, reg);
  reg = readReg(0x07);
  reg &= 0xC0;
  reg |= lrckH;
  ok &= writeReg(0x07, reg);
  ok &= writeReg(0x08, lrckL);
  return ok;
}

bool Es8311Codec::setBitsPerSample(uint8_t bits) {
  uint8_t code = 0;
  if (bits == 16) {
    code = 3;
  } else if (bits == 18) {
    code = 2;
  } else if (bits == 20) {
    code = 1;
  } else if (bits == 24) {
    code = 0;
  } else if (bits == 32) {
    code = 4;
  } else {
    return false;
  }
  bool ok = true;
  ok &= updateReg(0x09, 0x1C, code << 2);
  ok &= updateReg(0x0A, 0x1C, code << 2);
  return ok;
}

bool Es8311Codec::probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool Es8311Codec::updateReg(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = readReg(reg);
  current = (current & ~mask) | (value & mask);
  return writeReg(reg, current);
}

bool Es8311Codec::writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

uint8_t Es8311Codec::readReg(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

bool Es7210Adc::begin() {
  if (!probe(ES7210_ADDR)) {
    WALKIE_LOG_PRINTLN("[ES7210] not found");
    return false;
  }

  bool ok = true;
  ok &= writeReg(0x00, 0xFF);
  ok &= writeReg(0x00, 0x41);
  ok &= writeReg(0x01, 0x3F);
  ok &= writeReg(0x09, 0x30);
  ok &= writeReg(0x0A, 0x30);
  ok &= writeReg(0x23, 0x2A);
  ok &= writeReg(0x22, 0x0A);
  ok &= writeReg(0x20, 0x0A);
  ok &= writeReg(0x21, 0x2A);
  ok &= updateReg(0x08, 0x01, 0x00); // slave mode
  ok &= writeReg(0x40, 0x43);
  ok &= writeReg(0x41, 0x70);
  ok &= writeReg(0x42, 0x70);
  ok &= writeReg(0x07, 0x20);
  ok &= writeReg(0x02, 0xC1);
  ok &= selectAllMics();
  ok &= setAllMicGain(AUDIO_MIC_GAIN_REG_VALUE);
  ok &= setBits(16);
  ok &= configI2SFormat();
  offReg_ = readReg(0x01);
  ok &= start();
  WALKIE_LOG_PRINTF("[ES7210] regs 00=%02X 01=%02X 02=%02X 08=%02X 11=%02X 12=%02X 43=%02X 44=%02X 45=%02X 46=%02X\n",
                readReg(0x00),
                readReg(0x01),
                readReg(0x02),
                readReg(0x08),
                readReg(0x11),
                readReg(0x12),
                readReg(0x43),
                readReg(0x44),
                readReg(0x45),
                readReg(0x46));
  return ok;
}

bool Es7210Adc::probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool Es7210Adc::selectAllMics() {
  bool ok = true;
  for (uint8_t reg = 0x43; reg <= 0x46; reg++) {
    ok &= updateReg(reg, 0x10, 0x00);
  }
  ok &= writeReg(0x4B, 0xFF);
  ok &= writeReg(0x4C, 0xFF);

  ok &= updateReg(0x01, 0x0B, 0x00);
  ok &= writeReg(0x4B, 0x00);
  ok &= updateReg(0x43, 0x10, 0x10);
  ok &= updateReg(0x44, 0x10, 0x10);

  ok &= updateReg(0x01, 0x15, 0x00);
  ok &= writeReg(0x4C, 0x00);
  ok &= updateReg(0x45, 0x10, 0x10);
  ok &= updateReg(0x46, 0x10, 0x10);

  ok &= writeReg(0x12, 0x02); // TDM mode when 4 mics are selected
  return ok;
}

bool Es7210Adc::setAllMicGain(uint8_t gain) {
  bool ok = true;
  gain &= 0x0F;
  ok &= updateReg(0x43, 0x0F, gain);
  ok &= updateReg(0x44, 0x0F, gain);
  ok &= updateReg(0x45, 0x0F, gain);
  ok &= updateReg(0x46, 0x0F, gain);
  return ok;
}

bool Es7210Adc::setBits(uint8_t bits) {
  uint8_t value = 0x60;
  if (bits == 24) {
    value = 0x00;
  } else if (bits == 32) {
    value = 0x80;
  }
  return updateReg(0x11, 0xE0, value);
}

bool Es7210Adc::configI2SFormat() {
  return updateReg(0x11, 0x03, 0x00);
}

bool Es7210Adc::start() {
  bool ok = true;
  ok &= writeReg(0x01, offReg_);
  ok &= writeReg(0x06, 0x00);
  ok &= writeReg(0x40, 0x43);
  ok &= writeReg(0x47, 0x08);
  ok &= writeReg(0x48, 0x08);
  ok &= writeReg(0x49, 0x08);
  ok &= writeReg(0x4A, 0x08);
  ok &= selectAllMics();
  ok &= setAllMicGain(AUDIO_MIC_GAIN_REG_VALUE);
  ok &= writeReg(0x40, 0x43);
  ok &= writeReg(0x00, 0x71);
  ok &= writeReg(0x00, 0x41);
  return ok;
}

bool Es7210Adc::sleep() {
  bool ok = true;
  ok &= writeReg(0x47, 0xFF);
  ok &= writeReg(0x48, 0xFF);
  ok &= writeReg(0x49, 0xFF);
  ok &= writeReg(0x4A, 0xFF);
  ok &= writeReg(0x4B, 0xFF);
  ok &= writeReg(0x4C, 0xFF);
  ok &= writeReg(0x40, 0xC0);
  ok &= writeReg(0x01, 0x7F);
  ok &= writeReg(0x06, 0x07);
  return ok;
}

bool Es7210Adc::resume() {
  return start();
}

bool Es7210Adc::updateReg(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = readReg(reg);
  current = (current & ~mask) | (value & mask);
  return writeReg(reg, current);
}

bool Es7210Adc::writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES7210_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

uint8_t Es7210Adc::readReg(uint8_t reg) {
  Wire.beginTransmission(ES7210_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)ES7210_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}
