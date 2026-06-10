#include <RadioLib.h>
#include "EspHal.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define INITIATING_NODE

EspHal* hal = new EspHal(BSP_SPI_CLK, BSP_SPI_MISO, BSP_SPI_MOSI);
LR1121 radio = new Module(hal, BSP_SPI_CS, BSP_SPI_INT, BSP_SPI_RESET, BSP_SPI_BUSY);

static const char *TAG = "lr1121_pingpong";
static const char *PING_PAYLOAD = "Hello World!";

static const uint32_t rfswitch_dio_pins[] = {
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  { LR11x0::MODE_STBY,   { LOW,  LOW  } },
  { LR11x0::MODE_RX,     { HIGH, LOW  } },
  { LR11x0::MODE_TX,     { LOW,   HIGH } },
  { LR11x0::MODE_TX_HP,  { LOW,  HIGH } },
  { LR11x0::MODE_TX_HF,  { LOW,  LOW  } },
  { LR11x0::MODE_GNSS,   { LOW,  LOW  } },
  { LR11x0::MODE_WIFI,   { LOW,  LOW  } },
  END_OF_MODE_TABLE,
};

int transmissionState = RADIOLIB_ERR_NONE;
bool transmitFlag = false;
volatile bool operationDone = false;

void setFlag(void) {
  operationDone = true;
}

static void halt()
{
  while (true) {
    hal->delay(10);
  }
}

static void configure_radio()
{
  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);
  radio.setIrqAction(setFlag);
  radio.setFrequency(868.0);
  radio.setOutputPower(22);
  radio.setBandwidth(125.0);
  radio.setSpreadingFactor(7);
  radio.setCodingRate(5);
  radio.setSyncWord(RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE);
  radio.setPreambleLength(8);
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

  configure_radio();

#if defined(INITIATING_NODE)
  ESP_LOGI(TAG, "[LR1121] Sending first packet ...");
  transmissionState = radio.startTransmit(PING_PAYLOAD);
  transmitFlag = true;
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
    if(operationDone) {
      operationDone = false;
      if(transmitFlag) {
        if (transmissionState == RADIOLIB_ERR_NONE) {
          ESP_LOGI(TAG, "transmission finished!");
        } else {
          ESP_LOGI(TAG, "failed, code %d", transmissionState);
        }

        radio.startReceive();
        transmitFlag = false;

      } else {
        uint8_t rxBuffer[256];
        size_t rxLen = sizeof(rxBuffer);
        int state = radio.readData(rxBuffer, rxLen);

        if (state == RADIOLIB_ERR_NONE) {
          ESP_LOGI(TAG, "[LR1121] Received packet!");

          char strBuf[257] = {0};
          memcpy(strBuf, rxBuffer, rxLen);
          ESP_LOGI(TAG, "[LR1121] Data: %s", strBuf);

          ESP_LOGI(TAG, "[LR1121] RSSI: %.1f dBm", radio.getRSSI());
          ESP_LOGI(TAG, "[LR1121] SNR: %.1f dB", radio.getSNR());
        }

        hal->delay(1000);

        ESP_LOGI(TAG, "[LR1121] Sending another packet ...");
        transmissionState = radio.startTransmit(PING_PAYLOAD);
        transmitFlag = true;
      }
    }

    hal->delay(5);
  }
}
