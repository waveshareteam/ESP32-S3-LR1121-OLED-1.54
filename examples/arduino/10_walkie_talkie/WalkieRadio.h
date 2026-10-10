/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <RadioLib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// The bundled LR11x0 TX launch waits indefinitely for BUSY after PA ramp-up.
// Keep the workaround local to this sketch and let the worker report a timeout.
class WalkieRadio : public LR1121 {
public:
  explicit WalkieRadio(Module *module) : LR1121(module) {}

  int16_t launchMode() override {
    if (stagedMode != RADIOLIB_RADIO_MODE_TX) {
      return LR1121::launchMode();
    }
    Module *module = getMod();
    module->setRfSwitchState(Module::MODE_TX);
    int16_t state = setTx(RADIOLIB_LR11X0_TX_TIMEOUT_NONE);
    stagedMode = RADIOLIB_RADIO_MODE_NONE;
    if (state != RADIOLIB_ERR_NONE) {
      return state;
    }
    uint32_t startedAt = module->hal->millis();
    while (module->hal->digitalRead(module->getGpio())) {
      if (module->hal->millis() - startedAt >= 1000) {
        return RADIOLIB_ERR_SPI_CMD_TIMEOUT;
      }
      vTaskDelay(1);
    }
    return RADIOLIB_ERR_NONE;
  }
};
