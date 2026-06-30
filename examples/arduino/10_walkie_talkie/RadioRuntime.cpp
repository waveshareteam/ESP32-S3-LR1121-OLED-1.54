/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RadioRuntime.h"

#include <RadioLib.h>
#include <SPI.h>

#include "AppState.h"
#include "AudioPipeline.h"
#include "Config.h"
#include "Ui.h"

static LR1121 radio = new Module(PIN_LR_CS, PIN_LR_INT, PIN_LR_RST, PIN_LR_BUSY);

static volatile bool radioIrqFired = false;
static volatile bool radioReconfigureRequested = false;
static bool radioTxBusy = false;
static bool lastPttState = false;
static uint32_t lastTxWaitLog = 0;
static uint8_t radioPacket[LORA_TRANSMISSION_DATA_SIZE];

static const uint32_t rfSwitchDioPins[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC};

static const Module::RfSwitchMode_t rfSwitchTable[] = {
    {LR11x0::MODE_STBY, {LOW, LOW}},
    {LR11x0::MODE_RX, {HIGH, LOW}},
    {LR11x0::MODE_TX, {LOW, HIGH}},
    {LR11x0::MODE_TX_HP, {LOW, HIGH}},
    {LR11x0::MODE_TX_HF, {LOW, LOW}},
    {LR11x0::MODE_GNSS, {LOW, LOW}},
    {LR11x0::MODE_WIFI, {LOW, LOW}},
    END_OF_MODE_TABLE,
};

static void IRAM_ATTR onRadioIrq() {
  radioIrqFired = true;
}

bool radioRuntimeIsBusy() {
  return radioTxBusy || radioReconfigureRequested || radioIrqFired;
}

bool isRadioIrqAsserted() {
  return digitalRead(PIN_LR_INT) != LOW;
}

void clearRadioIrqPending() {
  radioIrqFired = false;
}

void setRadioIrqPending() {
  radioIrqFired = true;
}

void detachRadioIrqForSleep() {
  radio.clearIrqAction();
}

void restoreRadioIrqAction() {
  radio.setIrqAction(onRadioIrq);
}

int16_t startRadioReceiveForSleep() {
  return radio.startReceive();
}

void resetRadioRuntimeAfterSleep(bool pttPressed, bool irqPending) {
  lastPttState = pttPressed;
  transmissionMode = pttPressed;
  radioTxBusy = false;
  radioIrqFired = irqPending;
  lastTxWaitLog = millis();
}

void initRadioSpi() {
  SPI.begin(PIN_LR_SCK, PIN_LR_MISO, PIN_LR_MOSI, PIN_LR_CS);
}

void requestRadioReconfigure() {
  radioReconfigureRequested = true;
}

int16_t applyRadioConfig(bool startReceive) {
  radio.setRfSwitchTable(rfSwitchDioPins, rfSwitchTable);
  int16_t state = RADIOLIB_ERR_NONE;
  if (radioModem == RADIO_MODEM_GFSK) {
    state = radio.beginGFSK(radioFrequencyMhz,
                            radioGfskBitRateKbps,
                            radioGfskFreqDevKhz,
                            radioGfskRxBwKhz,
                            radioTxPowerDbm,
                            RADIO_GFSK_PREAMBLE_LENGTH,
                            RADIO_TCXO_VOLTAGE);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
    uint8_t gfskSyncWord[] = {0x01, 0x23, 0x45, 0x67, 0x89};
    state = radio.setSyncWord(gfskSyncWord, sizeof(gfskSyncWord));
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
  } else {
    state = radio.begin(radioFrequencyMhz,
                        radioLoraBandwidthKhz,
                        radioLoraSpreadingFactor,
                        radioLoraCodingRate,
                        RADIO_SYNC_WORD,
                        radioTxPowerDbm,
                        RADIO_PREAMBLE_LENGTH,
                        RADIO_TCXO_VOLTAGE);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
    state = radio.setCRC(0);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
  }

  restoreRadioIrqAction();
  return startReceive ? radio.startReceive() : radio.standby();
}

void handleRadioReconfigure() {
  if (!radioReconfigureRequested || radioTxBusy) {
    return;
  }
  radioReconfigureRequested = false;
  radioIrqFired = false;
  clearAudioBuffers();
  int16_t state = applyRadioConfig(!transmissionMode);
  if (state != RADIOLIB_ERR_NONE) {
    fatalError("Radio cfg", state);
  }
  Serial.printf("[LR1121] modem=%s freq=%.1f\n", radioModem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa", radioFrequencyMhz);
}

void handlePtt() {
  bool ptt = digitalRead(PIN_KEY_PTT) == LOW;
  if (ptt == lastPttState) {
    return;
  }
  lastPttState = ptt;
  if (ptt) {
    transmissionMode = true;
    clearAudioBuffers();
    radioIrqFired = false;
    if (!radioTxBusy) {
      int16_t state = radio.standby();
      if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[PTT] standby failed: %d\n", state);
      }
    }
    lastTxWaitLog = millis();
    Serial.println("[PTT] down, TX mode");
  } else {
    transmissionMode = false;
    if (!radioTxBusy) {
      int16_t state = radio.startReceive();
      if (state != RADIOLIB_ERR_NONE) {
        fatalError("Listen", state);
      }
    }
    clearAudioBuffers();
    Serial.println("[PTT] up, RX mode");
  }
  markUiDirty();
}

void handleRadioTransmission() {
  if (!transmissionMode || radioTxBusy) {
    return;
  }
  if (!audioPopTxPacket(radioPacket, LORA_TRANSMISSION_DATA_SIZE)) {
    uint32_t now = millis();
    if (now - lastTxWaitLog >= 1000) {
      lastTxWaitLog = now;
      Serial.printf("[LR1121] waiting TX packet queued=%uB frames=%u mic_fail=%u slot=%u\n",
                    (unsigned)audioTxQueuedBytes(),
                    (unsigned)audioTxEncodedFrames(),
                    (unsigned)audioMicReadFailures(),
                    audioSelectedMicSlot());
    }
    return;
  }

  int16_t state = radio.standby();
  if (state == RADIOLIB_ERR_NONE) {
    state = radio.startTransmit(radioPacket, LORA_TRANSMISSION_DATA_SIZE);
  }
  radioIrqFired = false;
  if (state == RADIOLIB_ERR_NONE) {
    radioTxBusy = true;
    Serial.printf("[LR1121] sending packet queued=%uB\n", (unsigned)audioTxQueuedBytes());
  } else {
    Serial.printf("[LR1121] transmit failed: %d\n", state);
  }
}

static void handleRxDone() {
  size_t rawLen = radio.getPacketLength();
  size_t len = rawLen;
  if (len > sizeof(radioPacket)) {
    len = sizeof(radioPacket);
  }
  if (len > 0) {
    int16_t readState = radio.readData(radioPacket, len);
    if (readState == RADIOLIB_ERR_NONE && rawLen == LORA_TRANSMISSION_DATA_SIZE) {
      audioPushRxPacket(radioPacket, LORA_TRANSMISSION_DATA_SIZE);
      Serial.println("[LR1121] packet received");
    } else {
      Serial.printf("[LR1121] drop packet len=%u state=%d\n", (unsigned)rawLen, readState);
    }
  }

  int16_t state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    fatalError("Listen", state);
  }
}

void handleRadioIrq() {
  if (!radioIrqFired) {
    return;
  }
  radioIrqFired = false;

  if (radioTxBusy) {
    int16_t state = radio.finishTransmit();
    radioTxBusy = false;
    if (state != RADIOLIB_ERR_NONE) {
      Serial.printf("[LR1121] finish TX failed: %d\n", state);
    }
    if (!transmissionMode) {
      state = radio.startReceive();
      if (state != RADIOLIB_ERR_NONE) {
        fatalError("Listen", state);
      }
    }
    return;
  }

  if (!transmissionMode) {
    handleRxDone();
  }
}
