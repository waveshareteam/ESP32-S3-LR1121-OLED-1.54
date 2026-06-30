/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Settings.h"

#include <Preferences.h>

#include "AppState.h"
#include "Config.h"
#include "RadioRuntime.h"
#include "Ui.h"

static Preferences prefs;

static uint8_t lastMd = RADIO_MODEM_GFSK;
static uint8_t lastBand = RADIO_BAND_HF;
static float lastFreqMhz = 868.0f;
static uint8_t lastSf = 2;
static uint8_t lastBw = 1;
static uint8_t lastCr = 1;
static uint8_t lastPwr = 6;
static uint8_t lastGfskBr = 1;
static uint8_t lastGfskDev = 1;
static uint8_t lastGfskRx = 1;

static const uint8_t sfValues[] = {5, 6, 7, 8, 9, 10, 11, 12};
static const float bwValues[] = {62.5f, 125.0f, 250.0f, 500.0f};
static const float gfskBrValues[] = {25.0f, 50.0f, 100.0f};
static const float gfskDevValues[] = {12.5f, 25.0f, 50.0f};
static const float gfskRxBwValues[] = {58.6f, 117.3f, 234.3f};
static const int8_t txPowerValues[] = {0, 5, 10, 14, 17, 20, 22};

static uint8_t normalizeBand(uint8_t band) {
  return band == RADIO_BAND_LF ? RADIO_BAND_LF : RADIO_BAND_HF;
}

static uint16_t radioBandMinFreq(uint8_t band) {
  return normalizeBand(band) == RADIO_BAND_LF ? RADIO_LF_MIN_FREQ_MHZ : RADIO_HF_MIN_FREQ_MHZ;
}

static uint16_t radioBandMaxFreq(uint8_t band) {
  return normalizeBand(band) == RADIO_BAND_LF ? RADIO_LF_MAX_FREQ_MHZ : RADIO_HF_MAX_FREQ_MHZ;
}

static uint16_t radioBandDefaultFreq(uint8_t band) {
  return normalizeBand(band) == RADIO_BAND_LF ? RADIO_LF_DEFAULT_FREQ_MHZ : RADIO_HF_DEFAULT_FREQ_MHZ;
}

static uint8_t radioBandFreqStep(uint8_t band) {
  return normalizeBand(band) == RADIO_BAND_LF ? RADIO_LF_FREQ_STEP_MHZ : RADIO_HF_FREQ_STEP_MHZ;
}

static uint8_t radioBandFreqCount(uint8_t band) {
  uint16_t minFreq = radioBandMinFreq(band);
  uint16_t maxFreq = radioBandMaxFreq(band);
  uint8_t step = radioBandFreqStep(band);
  return (uint8_t)(((maxFreq - minFreq + step - 1) / step) + 1);
}

static bool radioFrequencyInBand(uint8_t band, uint16_t freqMhz) {
  return freqMhz >= radioBandMinFreq(band) && freqMhz <= radioBandMaxFreq(band);
}

static float radioIndexToFreq(uint8_t index) {
  uint8_t band = normalizeBand(lastBand);
  uint16_t freq = radioBandMinFreq(band) + index * radioBandFreqStep(band);
  uint16_t maxFreq = radioBandMaxFreq(band);
  if (freq > maxFreq) {
    freq = maxFreq;
  }
  return (float)freq;
}

static void radioSetFrequencyMhz(float freqMhz) {
  uint8_t band = normalizeBand(lastBand);
  uint16_t freq = (uint16_t)(freqMhz + 0.5f);
  if (!radioFrequencyInBand(band, freq)) {
    freq = radioBandDefaultFreq(band);
  }
  lastFreqMhz = radioIndexToFreq((uint8_t)((freq - radioBandMinFreq(band)) / radioBandFreqStep(band)));
}

static void radioSelectBand(uint8_t band) {
  lastBand = normalizeBand(band);
  radioSetFrequencyMhz(lastFreqMhz);
}

const char *radioBandLabel() {
  return radioBand == RADIO_BAND_LF ? "LF" : "HF";
}

uint8_t optionCount(uint8_t page) {
  switch (page) {
  case 0:
    return 2;
  case 1:
    return radioBandFreqCount(lastBand);
  case 2:
    return radioModem == RADIO_MODEM_LORA ? countOf(sfValues) : countOf(gfskBrValues);
  case 3:
    return radioModem == RADIO_MODEM_LORA ? countOf(bwValues) : countOf(gfskDevValues);
  case 4:
    return radioModem == RADIO_MODEM_LORA ? 8 : countOf(gfskRxBwValues);
  case 5:
    return countOf(txPowerValues);
  case 6:
    return 2;
  default:
    return 1;
  }
}

static uint8_t radioFreqToIndex(float freqMhz) {
  uint8_t band = normalizeBand(lastBand);
  uint16_t minFreq = radioBandMinFreq(band);
  uint16_t maxFreq = radioBandMaxFreq(band);
  uint8_t step = radioBandFreqStep(band);
  int freq = (int)(freqMhz + 0.5f);
  if (freq < minFreq) {
    return 0;
  }
  if (freq >= maxFreq) {
    return radioBandFreqCount(band) - 1;
  }
  return (uint8_t)((freq - minFreq) / step);
}

static void formatDecimal(char *out, size_t outSize, float value) {
  int valueX10 = (int)(value * 10.0f + 0.5f);
  if ((valueX10 % 10) == 0) {
    snprintf(out, outSize, "%d", valueX10 / 10);
  } else {
    snprintf(out, outSize, "%d.%d", valueX10 / 10, valueX10 % 10);
  }
}

static const char *formatLoraBandwidth(float bandwidth) {
  int bwX10 = (int)(bandwidth * 10.0f + 0.5f);
  switch (bwX10) {
  case 625:
    return "62.5";
  case 1250:
    return "125";
  case 2500:
    return "250";
  case 5000:
    return "500";
  default:
    return "?";
  }
}

void formatParams(char *out, size_t outSize) {
  if (radioModem == RADIO_MODEM_LORA) {
    snprintf(out, outSize, "SF%u BW%s CR4/%u P%d",
             radioLoraSpreadingFactor,
             formatLoraBandwidth(radioLoraBandwidthKhz),
             radioLoraCodingRate,
             (int)radioTxPowerDbm);
  } else {
    char br[12];
    char dev[12];
    char rx[12];
    formatDecimal(br, sizeof(br), radioGfskBitRateKbps);
    formatDecimal(dev, sizeof(dev), radioGfskFreqDevKhz);
    formatDecimal(rx, sizeof(rx), radioGfskRxBwKhz);
    snprintf(out, outSize, "BR%s DEV%s RX%s P%d", br, dev, rx, (int)radioTxPowerDbm);
  }
}

static void applyIndexesToRuntime() {
  radioModem = lastMd == RADIO_MODEM_LORA ? RADIO_MODEM_LORA : RADIO_MODEM_GFSK;
  radioBand = normalizeBand(lastBand);
  radioFrequencyMhz = lastFreqMhz;
  radioLoraSpreadingFactor = sfValues[lastSf];
  radioLoraBandwidthKhz = bwValues[lastBw];
  radioLoraCodingRate = lastCr + 4;
  radioGfskBitRateKbps = gfskBrValues[lastGfskBr];
  radioGfskFreqDevKhz = gfskDevValues[lastGfskDev];
  radioGfskRxBwKhz = gfskRxBwValues[lastGfskRx];
  radioTxPowerDbm = txPowerValues[lastPwr];
}

void loadRadioSettings() {
  prefs.begin("radio", true);
  uint8_t value = prefs.getUChar("mode", lastMd);
  if (value <= RADIO_MODEM_LORA) {
    lastMd = value;
  }

  value = prefs.getUChar("band", lastBand);
  if (value <= RADIO_BAND_LF) {
    lastBand = value;
  }
  radioSelectBand(lastBand);

  uint16_t freq = prefs.getUShort("freq", (uint16_t)lastFreqMhz);
  radioSetFrequencyMhz((float)freq);

  value = prefs.getUChar("sf", lastSf);
  if (value < countOf(sfValues)) {
    lastSf = value;
  }

  value = prefs.getUChar("bw", lastBw);
  if (value < countOf(bwValues)) {
    lastBw = value;
  }

  value = prefs.getUChar("cr", lastCr);
  if (value < 8) {
    lastCr = value;
  }

  value = prefs.getUChar("gbr", lastGfskBr);
  if (value < countOf(gfskBrValues)) {
    lastGfskBr = value;
  }

  value = prefs.getUChar("gdev", lastGfskDev);
  if (value < countOf(gfskDevValues)) {
    lastGfskDev = value;
  }

  value = prefs.getUChar("grx", lastGfskRx);
  if (value < countOf(gfskRxBwValues)) {
    lastGfskRx = value;
  }

  value = prefs.getUChar("pwr", lastPwr);
  if (value < countOf(txPowerValues)) {
    lastPwr = value;
  }
  prefs.end();
  applyIndexesToRuntime();
}

static void saveRadioSettings() {
  prefs.begin("radio", false);
  prefs.putUChar("mode", lastMd);
  prefs.putUChar("band", lastBand);
  prefs.putUShort("freq", (uint16_t)(lastFreqMhz + 0.5f));
  prefs.putUChar("sf", lastSf);
  prefs.putUChar("bw", lastBw);
  prefs.putUChar("cr", lastCr);
  prefs.putUChar("gbr", lastGfskBr);
  prefs.putUChar("gdev", lastGfskDev);
  prefs.putUChar("grx", lastGfskRx);
  prefs.putUChar("pwr", lastPwr);
  prefs.end();
}

uint8_t currentSettingIndex(uint8_t page) {
  switch (page) {
  case 0:
    return lastMd;
  case 1:
    return radioFreqToIndex(lastFreqMhz);
  case 2:
    return radioModem == RADIO_MODEM_LORA ? lastSf : lastGfskBr;
  case 3:
    return radioModem == RADIO_MODEM_LORA ? lastBw : lastGfskDev;
  case 4:
    return radioModem == RADIO_MODEM_LORA ? lastCr : lastGfskRx;
  case 5:
    return lastPwr;
  case 6:
    return lastBand;
  default:
    return 0;
  }
}

const char *settingTitle(uint8_t page) {
  switch (page) {
  case 0:
    return "MODE";
  case 1:
    return "FREQ";
  case 2:
    return radioModem == RADIO_MODEM_LORA ? "SPREAD" : "BITRATE";
  case 3:
    return radioModem == RADIO_MODEM_LORA ? "BAND" : "DEVIAT";
  case 4:
    return radioModem == RADIO_MODEM_LORA ? "CODING" : "RXBW";
  case 5:
    return "PWR";
  case 6:
    return "BAND";
  default:
    return "";
  }
}

void settingOptionText(uint8_t page, uint8_t index, char *out, size_t outSize) {
  switch (page) {
  case 0:
    snprintf(out, outSize, "%s", index == 0 ? "GFSK" : "LoRa");
    break;
  case 1:
    snprintf(out, outSize, "%u", (unsigned)(uint16_t)radioIndexToFreq(index));
    break;
  case 2:
    if (radioModem == RADIO_MODEM_LORA) {
      snprintf(out, outSize, "SF%u", sfValues[index]);
    } else {
      char v[12];
      formatDecimal(v, sizeof(v), gfskBrValues[index]);
      snprintf(out, outSize, "%s kbps", v);
    }
    break;
  case 3:
    if (radioModem == RADIO_MODEM_LORA) {
      snprintf(out, outSize, "%s kHz", formatLoraBandwidth(bwValues[index]));
    } else {
      char v[12];
      formatDecimal(v, sizeof(v), gfskDevValues[index]);
      snprintf(out, outSize, "%s kHz", v);
    }
    break;
  case 4:
    if (radioModem == RADIO_MODEM_LORA) {
      static const char *cr[] = {"NO CR", "CR 45", "CR 46", "CR 47", "CR 48", "CR LI 45", "CR LI 46", "CR LI 48"};
      snprintf(out, outSize, "%s", cr[index]);
    } else {
      char v[12];
      formatDecimal(v, sizeof(v), gfskRxBwValues[index]);
      snprintf(out, outSize, "%s kHz", v);
    }
    break;
  case 5:
    snprintf(out, outSize, "%d dBm", (int)txPowerValues[index]);
    break;
  case 6:
    snprintf(out, outSize, "%s", index == RADIO_BAND_LF ? "LF" : "HF");
    break;
  default:
    out[0] = '\0';
    break;
  }
}

void applySettingSelection(uint8_t page, uint8_t index) {
  switch (page) {
  case 0:
    lastMd = index;
    break;
  case 1:
    radioSetFrequencyMhz(radioIndexToFreq(index));
    break;
  case 2:
    if (radioModem == RADIO_MODEM_LORA) {
      lastSf = index;
    } else {
      lastGfskBr = index;
    }
    break;
  case 3:
    if (radioModem == RADIO_MODEM_LORA) {
      lastBw = index;
    } else {
      lastGfskDev = index;
    }
    break;
  case 4:
    if (radioModem == RADIO_MODEM_LORA) {
      lastCr = index;
    } else {
      lastGfskRx = index;
    }
    break;
  case 5:
    lastPwr = index;
    break;
  case 6:
    radioSelectBand(index);
    break;
  }
  applyIndexesToRuntime();
  saveRadioSettings();
  requestRadioReconfigure();
  markUiDirty();
}
