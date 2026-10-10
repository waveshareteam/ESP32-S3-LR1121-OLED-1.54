/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RadioRuntime.h"

#include <RadioLib.h>
#include <SPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "AppState.h"
#include "AudioPipeline.h"
#include "Config.h"
#include "Settings.h"
#include "Ui.h"
#include "WalkieRadio.h"

static WalkieRadio radio(new Module(PIN_LR_CS, PIN_LR_INT, PIN_LR_RST, PIN_LR_BUSY));

static volatile bool radioIrqFired = false;
// These flags belong to the Arduino loop; the queues transfer worker ownership.
static bool radioReconfigureRequested = false;
static bool radioWorkerBusy = false;
static uint8_t configuredModem = 0xFF;
static bool radioTxBusy = false;
static bool lastPttState = false;
static bool radioModeNeedsSync = false;
static bool radioRetryPending = false;
static uint32_t radioRetryAt = 0;
static uint32_t radioTxStartedAt = 0;
static uint32_t radioTxTimeoutMs = 0;
static uint8_t radioPacket[LORA_TRANSMISSION_DATA_SIZE];
// Written by the worker and sampled by the loop after receiving its result.
static uint32_t txStartedPackets = 0;
static uint32_t txFinishedPackets = 0;
static uint32_t rxAcceptedPackets = 0;
static uint32_t rxDroppedPackets = 0;
static uint32_t lastTxAirTimeMs = 0;
static int16_t lastPacketReadState = RADIOLIB_ERR_NONE;
static size_t lastPacketLength = 0;
static uint32_t txCompletionTimeouts = 0;

struct RadioConfiguration {
  uint8_t modem;
  float frequencyMhz, loraBandwidthKhz;
  uint8_t loraSpreadingFactor, loraCodingRate;
  float gfskBitRateKbps, gfskFreqDevKhz, gfskRxBwKhz;
  int8_t txPowerDbm;
};

enum class RadioOperation : uint8_t { Configure, SetPtt, StartTx, FinishTx, Receive };

struct RadioRequest {
  RadioOperation operation;
  RadioConfiguration config;
  RadioSettingsSnapshot settings;
  bool transmitting;
};

struct RadioResult {
  RadioOperation operation;
  int16_t state;
  bool transmitting;
  uint32_t txStartedAt;
  uint32_t txTimeoutMs;
};

static RadioConfiguration configuredSettings;
static QueueHandle_t radioRequests = nullptr;
static QueueHandle_t radioResults = nullptr;

static RadioConfiguration captureRadioConfiguration() {
  return {radioModem, radioFrequencyMhz, radioLoraBandwidthKhz,
          radioLoraSpreadingFactor, radioLoraCodingRate, radioGfskBitRateKbps,
          radioGfskFreqDevKhz, radioGfskRxBwKhz, radioTxPowerDbm};
}

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
  return radioTxBusy || radioReconfigureRequested || radioWorkerBusy ||
         radioModeNeedsSync || radioRetryPending || radioIrqFired;
}

bool isRadioIrqAsserted() {
  return !radioWorkerBusy && digitalRead(PIN_LR_INT) != LOW;
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
  radioModeNeedsSync = false;
  radioRetryPending = false;
}

void initRadioSpi() {
  SPI.begin(PIN_LR_SCK, PIN_LR_MISO, PIN_LR_MOSI, PIN_LR_CS);
}

void requestRadioReconfigure() {
  radioReconfigureRequested = true;
}

static int16_t updateRadioParameters(const RadioConfiguration &config) {
  // Stop RX before updating settings, without resetting or recalibrating all blocks.
  int16_t state = radio.standby();
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }
  state = radio.clearIrqFlags(RADIOLIB_LR11X0_IRQ_ALL);
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }
  radioIrqFired = false;

  if (config.modem == RADIO_MODEM_GFSK) {
    if (config.gfskBitRateKbps != configuredSettings.gfskBitRateKbps) {
      state = radio.setBitRate(config.gfskBitRateKbps);
    }
    if (state == RADIOLIB_ERR_NONE && config.gfskFreqDevKhz != configuredSettings.gfskFreqDevKhz) {
      state = radio.setFrequencyDeviation(config.gfskFreqDevKhz);
    }
    if (state == RADIOLIB_ERR_NONE && config.gfskRxBwKhz != configuredSettings.gfskRxBwKhz) {
      state = radio.setRxBandwidth(config.gfskRxBwKhz);
    }
  } else {
    if (config.loraBandwidthKhz != configuredSettings.loraBandwidthKhz) {
      state = radio.setBandwidth(config.loraBandwidthKhz);
    }
    if (state == RADIOLIB_ERR_NONE && config.loraSpreadingFactor != configuredSettings.loraSpreadingFactor) {
      state = radio.setSpreadingFactor(config.loraSpreadingFactor);
    }
    if (state == RADIOLIB_ERR_NONE && config.loraCodingRate != configuredSettings.loraCodingRate) {
      state = radio.setCodingRate(config.loraCodingRate);
    }
  }
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  // Keep RadioLib's frequency-dependent image calibration and PA selection.
  bool frequencyChanged = config.frequencyMhz != configuredSettings.frequencyMhz;
  if (frequencyChanged) {
    state = radio.setFrequency(config.frequencyMhz);
  }
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }
  if (frequencyChanged || config.txPowerDbm != configuredSettings.txPowerDbm) {
    state = radio.setOutputPower(config.txPowerDbm);
  }
  return state;
}

static int16_t applyRadioConfiguration(const RadioConfiguration &config, bool startReceive) {
  if (configuredModem == config.modem) {
    int16_t state = updateRadioParameters(config);
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
    state = startReceive ? radio.startReceive() : radio.standby();
    if (state == RADIOLIB_ERR_NONE) {
      configuredSettings = config;
    }
    return state;
  }

  // A full initialization is only needed at startup or when changing modem.
  radio.setRfSwitchTable(rfSwitchDioPins, rfSwitchTable);
  int16_t state = RADIOLIB_ERR_NONE;
  if (config.modem == RADIO_MODEM_GFSK) {
    state = radio.beginGFSK(config.frequencyMhz,
                            config.gfskBitRateKbps,
                            config.gfskFreqDevKhz,
                            config.gfskRxBwKhz,
                            config.txPowerDbm,
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
    state = radio.begin(config.frequencyMhz,
                        config.loraBandwidthKhz,
                        config.loraSpreadingFactor,
                        config.loraCodingRate,
                        RADIO_SYNC_WORD,
                        config.txPowerDbm,
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
  radioIrqFired = false;
  state = startReceive ? radio.startReceive() : radio.standby();
  if (state == RADIOLIB_ERR_NONE) {
    configuredModem = config.modem;
    configuredSettings = config;
  }
  return state;
}

int16_t applyRadioConfig(bool startReceive) {
  // Used only by setup, before the background task starts.
  return applyRadioConfiguration(captureRadioConfiguration(), startReceive);
}

// Runs only on the worker. The loop never touches radioPacket while a job is
// in flight, and only the worker accesses SPI between submission and completion.
static RadioResult executeRadioRequest(const RadioRequest &request) {
  RadioResult result = {request.operation, RADIOLIB_ERR_NONE, request.transmitting, 0, 0};
  switch (request.operation) {
    case RadioOperation::Configure: {
      uint32_t startedAt = millis();
      bool fullInit = configuredModem != request.config.modem;
      result.state = applyRadioConfiguration(request.config, !request.transmitting);
      uint32_t configuredAt = millis();
      if (result.state == RADIOLIB_ERR_NONE && !saveRadioSettings(request.settings)) {
        WALKIE_LOG_PRINTLN("[Settings] save failed");
      }
      WALKIE_LOG_PRINTF("[LR1121] modem=%s freq=%.1f config=%s elapsed=%ums save=%ums\n",
                    request.config.modem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa",
                    request.config.frequencyMhz, fullInit ? "full" : "update",
                    (unsigned)(configuredAt - startedAt), (unsigned)(millis() - configuredAt));
      break;
    }
    case RadioOperation::SetPtt:
      result.state = radio.standby();
      if (result.state == RADIOLIB_ERR_NONE) {
        result.state = radio.clearIrqFlags(RADIOLIB_LR11X0_IRQ_ALL);
      }
      if (result.state == RADIOLIB_ERR_NONE && !request.transmitting) {
        result.state = radio.startReceive();
      }
      WALKIE_LOG_PRINTLN(request.transmitting ? "[PTT] down, TX mode" : "[PTT] up, RX mode");
      break;
    case RadioOperation::StartTx:
      result.state = radio.standby();
      if (result.state == RADIOLIB_ERR_NONE) {
        // A slow LoRa setting can legitimately take seconds per packet.
        // Use its actual airtime, with a margin, instead of a fixed TX deadline.
        result.txTimeoutMs = (uint32_t)((uint64_t)radio.getTimeOnAir(LORA_TRANSMISSION_DATA_SIZE) * 3 / 2000) + 1000;
        lastTxAirTimeMs = (result.txTimeoutMs - 1000) * 2 / 3;
        result.txStartedAt = millis();
        result.state = radio.startTransmit(radioPacket, LORA_TRANSMISSION_DATA_SIZE);
        if (result.state == RADIOLIB_ERR_NONE) {
          txStartedPackets++;
        }
      }
      break;
    case RadioOperation::FinishTx:
      result.state = radio.finishTransmit();
      if (result.state == RADIOLIB_ERR_NONE) {
        txFinishedPackets++;
      }
      if (result.state == RADIOLIB_ERR_NONE && !request.transmitting) {
        result.state = radio.startReceive();
      }
      break;
    case RadioOperation::Receive: {
      size_t rawLen = radio.getPacketLength();
      lastPacketLength = rawLen;
      size_t len = rawLen > sizeof(radioPacket) ? sizeof(radioPacket) : rawLen;
      bool accepted = false;
      lastPacketReadState = RADIOLIB_ERR_NONE;
      if (len > 0) {
        int16_t readState = radio.readData(radioPacket, len);
        lastPacketReadState = readState;
        if (readState == RADIOLIB_ERR_NONE && rawLen == LORA_TRANSMISSION_DATA_SIZE && !transmissionMode) {
          audioPushRxPacket(radioPacket, LORA_TRANSMISSION_DATA_SIZE);
          rxAcceptedPackets++;
          accepted = true;
        }
      }
      if (!accepted) {
        rxDroppedPackets++;
        if (rxDroppedPackets <= 3 || rxDroppedPackets % 10 == 0) {
          WALKIE_LOG_PRINTF("[LR1121] RX dropped: len=%u read=%d ptt=%u count=%u\n",
                        unsigned(rawLen), lastPacketReadState, unsigned(transmissionMode), unsigned(rxDroppedPackets));
        }
      }
      result.state = radio.startReceive();
      break;
    }
  }
  if (result.state != RADIOLIB_ERR_NONE) {
    WALKIE_LOG_PRINTF("[LR1121] operation=%u failed: %d (busy=%d irq=%d)\n",
                  (unsigned)request.operation, result.state,
                  digitalRead(PIN_LR_BUSY), digitalRead(PIN_LR_INT));
  }
  return result;
}

static void radioTask(void *) {
  RadioRequest request;
  while (true) {
    if (xQueueReceive(radioRequests, &request, portMAX_DELAY) == pdTRUE) {
      RadioResult result = executeRadioRequest(request);
      xQueueSend(radioResults, &result, portMAX_DELAY);
    }
  }
}

bool startRadioConfigTask() {
  radioRequests = xQueueCreate(1, sizeof(RadioRequest));
  radioResults = xQueueCreate(1, sizeof(RadioResult));
  if (radioRequests && radioResults &&
      xTaskCreatePinnedToCore(radioTask, "Radio_Worker", 6144, nullptr, 1, nullptr,
                             (xPortGetCoreID() + 1) % portNUM_PROCESSORS) == pdPASS) {
    return true;
  }
  if (radioRequests) {
    vQueueDelete(radioRequests);
    radioRequests = nullptr;
  }
  if (radioResults) {
    vQueueDelete(radioResults);
    radioResults = nullptr;
  }
  return false;
}

static bool submitRadioRequest(const RadioRequest &request) {
  if (radioWorkerBusy) {
    return false;
  }
  // Clear before launch: a fast TX-done IRQ may arrive before the job returns.
  radioIrqFired = false;
  radioWorkerBusy = true;
  if (xQueueSend(radioRequests, &request, 0) == pdTRUE) {
    return true;
  }
  radioWorkerBusy = false;
  return false;
}

void handleRadioReconfigure() {
  if (radioWorkerBusy) {
    RadioResult result;
    if (xQueueReceive(radioResults, &result, 0) != pdTRUE) {
      return;
    }
    radioWorkerBusy = false;
    if (result.operation == RadioOperation::StartTx) {
      radioTxBusy = result.state == RADIOLIB_ERR_NONE;
      radioTxStartedAt = result.txStartedAt;
      radioTxTimeoutMs = result.txTimeoutMs;
    } else if (result.operation == RadioOperation::FinishTx) {
      radioTxBusy = false;
    }
    radioModeNeedsSync = result.transmitting != transmissionMode;
    if (result.state != RADIOLIB_ERR_NONE) {
      // Keep the UI alive on an RF fault; retry off the UI thread, at most once
      // per second. A failed configuration must be fully reapplied before TX.
      if (result.operation == RadioOperation::Configure) {
        configuredModem = 0xFF;
        radioReconfigureRequested = true;
      }
      radioModeNeedsSync = true;
      radioRetryPending = true;
      radioRetryAt = millis();
    }
  }
  if (radioWorkerBusy || radioTxBusy) {
    return;
  }
  if (radioRetryPending) {
    if (millis() - radioRetryAt < 1000) {
      return;
    }
    radioRetryPending = false;
  }
  if (radioReconfigureRequested) {
    RadioRequest request = {RadioOperation::Configure, captureRadioConfiguration(), captureRadioSettings(), transmissionMode};
    clearAudioBuffers();
    if (submitRadioRequest(request)) {
      radioReconfigureRequested = false;
    }
  } else if (radioModeNeedsSync) {
    RadioRequest request = {};
    request.operation = RadioOperation::SetPtt;
    request.transmitting = transmissionMode;
    submitRadioRequest(request);
  }
}

void handlePtt() {
  bool ptt = digitalRead(PIN_KEY_PTT) == LOW;
  if (ptt == lastPttState) {
    return;
  }
  lastPttState = ptt;
  transmissionMode = ptt;
  radioModeNeedsSync = true;
  clearAudioBuffers();
  markUiDirty();
}

void handleRadioTransmission() {
  if (!transmissionMode || radioTxBusy || radioWorkerBusy || radioModeNeedsSync ||
      radioRetryPending || radioReconfigureRequested) {
    return;
  }
  if (!audioPopTxPacket(radioPacket, LORA_TRANSMISSION_DATA_SIZE)) {
    return;
  }
  RadioRequest request = {};
  request.operation = RadioOperation::StartTx;
  request.transmitting = true;
  submitRadioRequest(request);
}

void handleRadioIrq() {
  if (radioWorkerBusy) {
    return;
  }
  bool txTimedOut = radioTxBusy && millis() - radioTxStartedAt >= radioTxTimeoutMs;
  if (!radioIrqFired && !isRadioIrqAsserted() && !txTimedOut) {
    return;
  }
  RadioRequest request = {};
  request.transmitting = transmissionMode;
  if (radioTxBusy) {
    if (txTimedOut) {
      txCompletionTimeouts++;
      WALKIE_LOG_PRINTLN("[LR1121] TX completion timeout, recovering");
    }
    request.operation = RadioOperation::FinishTx;
    submitRadioRequest(request);
  } else if (!transmissionMode && !radioModeNeedsSync && !radioRetryPending && !radioReconfigureRequested) {
    request.operation = RadioOperation::Receive;
    submitRadioRequest(request);
  } else {
    radioIrqFired = false;
  }
}

void logRadioDiagnostics() {
  static uint32_t lastLog = 0;
  uint32_t now = millis();
  // Do not read worker-owned state until the queue has handed ownership back.
  if (!VOICE_DEBUG_LOG || radioWorkerBusy || now - lastLog < 2000) {
    return;
  }
  lastLog = now;
  const auto &config = configuredSettings;
  if (configuredModem == RADIO_MODEM_LORA) {
    WALKIE_LOG_PRINTF("[LinkConfig] LoRa freq=%.1f SF=%u BW=%.1f CR=4/%u sync=0x%02x power=%d\n",
                  config.frequencyMhz, config.loraSpreadingFactor, config.loraBandwidthKhz,
                  config.loraCodingRate, RADIO_SYNC_WORD, config.txPowerDbm);
  } else {
    WALKIE_LOG_PRINTF("[LinkConfig] GFSK freq=%.1f BR=%.1f DEV=%.1f RXBW=%.1f power=%d\n",
                  config.frequencyMhz, config.gfskBitRateKbps, config.gfskFreqDevKhz,
                  config.gfskRxBwKhz, config.txPowerDbm);
  }
  WALKIE_LOG_PRINTF("[Link] ptt=%u tx_start=%u tx_finish=%u tx_timeout=%u airtime=%ums "
                "rx_ok=%u rx_drop=%u last_len=%u last_read=%d irq=%d busy=%d\n",
                unsigned(transmissionMode), unsigned(txStartedPackets), unsigned(txFinishedPackets),
                unsigned(txCompletionTimeouts), unsigned(lastTxAirTimeMs), unsigned(rxAcceptedPackets),
                unsigned(rxDroppedPackets), unsigned(lastPacketLength), lastPacketReadState,
                digitalRead(PIN_LR_INT), digitalRead(PIN_LR_BUSY));
}
