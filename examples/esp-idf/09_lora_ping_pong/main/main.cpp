/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <RadioLib.h>
#include "EspHal.h"

#include <string.h>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define INITIATING_NODE

static const char *TAG = "lr1121_ping_pong";
static const char *PING_PAYLOAD = "Hello World!";

static EspHal hal;
static Module radio_module(&hal, BSP_SPI_CS, BSP_SPI_INT, BSP_SPI_RESET, BSP_SPI_BUSY);
static LR1121 radio(&radio_module);

#include "board_radio.h"

static int transmission_state = RADIOLIB_ERR_NONE;
static bool transmit_flag = false;
static volatile bool operation_done = false;

static void set_flag(void)
{
  operation_done = true;
}

static void halt()
{
  while (true) {
    hal.delay(10);
  }
}

static bool check_radio_state(int state, const char *operation)
{
  if (state == RADIOLIB_ERR_NONE) {
    return true;
  }

  ESP_LOGE(TAG, "%s failed, code %d", operation, state);
  return false;
}

static bool configure_radio()
{
  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);
  radio.setIrqAction(set_flag);
  return check_radio_state(radio.setFrequency(868.0), "setFrequency") &&
         check_radio_state(radio.setOutputPower(22), "setOutputPower") &&
         check_radio_state(radio.setBandwidth(125.0), "setBandwidth") &&
         check_radio_state(radio.setSpreadingFactor(7), "setSpreadingFactor") &&
         check_radio_state(radio.setCodingRate(5), "setCodingRate") &&
         check_radio_state(radio.setSyncWord(RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE), "setSyncWord") &&
         check_radio_state(radio.setPreambleLength(8), "setPreambleLength");
}

extern "C" void app_main(void) {

  ESP_LOGI(TAG, "[LR1121] Initializing ... ");
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    ESP_LOGI(TAG, "success!");
  } else {
    ESP_LOGI(TAG, "failed, code %d", state);
    halt();
  }

  if (!configure_radio()) {
    halt();
  }

#if defined(INITIATING_NODE)
  ESP_LOGI(TAG, "[LR1121] Sending first packet ...");
  transmission_state = radio.startTransmit(PING_PAYLOAD);
  if (!check_radio_state(transmission_state, "startTransmit")) {
    halt();
  }
  transmit_flag = true;
#else
  ESP_LOGI(TAG, "[LR1121] Starting to listen ...");
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    ESP_LOGI(TAG, "success!");
  } else {
    ESP_LOGI(TAG, "failed, code %d", state);
    halt();
  }
#endif

  while (1)
  {
    if (operation_done) {
      operation_done = false;
      if (transmit_flag) {
        if (transmission_state == RADIOLIB_ERR_NONE) {
          ESP_LOGI(TAG, "transmission finished!");
        } else {
          ESP_LOGI(TAG, "failed, code %d", transmission_state);
        }

        int state = radio.startReceive();
        if (!check_radio_state(state, "startReceive")) {
          halt();
        }
        transmit_flag = false;

      } else {
        uint8_t rxBuffer[256];
        size_t rxLen = std::min(radio.getPacketLength(), sizeof(rxBuffer));
        int state = radio.readData(rxBuffer, rxLen);

        if (state == RADIOLIB_ERR_NONE) {
          ESP_LOGI(TAG, "[LR1121] Received packet!");

          char strBuf[257] = {0};
          memcpy(strBuf, rxBuffer, rxLen);
          ESP_LOGI(TAG, "[LR1121] Data: %s", strBuf);

          ESP_LOGI(TAG, "[LR1121] RSSI: %.1f dBm", radio.getRSSI());
          ESP_LOGI(TAG, "[LR1121] SNR: %.1f dB", radio.getSNR());
        }

        hal.delay(1000);

        ESP_LOGI(TAG, "[LR1121] Sending another packet ...");
        transmission_state = radio.startTransmit(PING_PAYLOAD);
        if (!check_radio_state(transmission_state, "startTransmit")) {
          halt();
        }
        transmit_flag = true;
      }
    }

    hal.delay(5);
  }
}
