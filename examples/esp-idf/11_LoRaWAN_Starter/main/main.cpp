#include "EspHal.h"

EspHal* hal = new EspHal(BSP_SPI_CLK, BSP_SPI_MISO, BSP_SPI_MOSI);
LR1121 radio = new Module(hal, BSP_SPI_CS, BSP_SPI_INT, BSP_SPI_RESET, BSP_SPI_BUSY);

LoRaWANNode node(&radio, &Region, subBand);

static const char *TAG = "main";

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

static nvs_handle_t nvsHandle;

static const char* NVS_NAMESPACE     = "lorawan";
static const char* NVS_KEY_DEV_NONCE = "nonce";
static const char* NVS_KEY_SESSION   = "sess";

static void halt()
{
    while (true) {
        hal->delay(10);
    }
}

static void saveLoRaWANState() {
    nvs_set_blob(nvsHandle, NVS_KEY_DEV_NONCE,
                 node.getBufferNonces(),
                 RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

    nvs_set_blob(nvsHandle, NVS_KEY_SESSION,
                 node.getBufferSession(),
                 RADIOLIB_LORAWAN_SESSION_BUF_SIZE);

    nvs_commit(nvsHandle);
    ESP_LOGI(TAG, "[NVS] State saved");
}

static void restoreLoRaWANState() {
    uint8_t buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    size_t bufSize = sizeof(buffer);

    esp_err_t ret = nvs_get_blob(nvsHandle, NVS_KEY_DEV_NONCE, buffer, &bufSize);
    if (ret == ESP_OK) {
        node.setBufferNonces(buffer);
        ESP_LOGI(TAG, "[NVS] Nonce restored");
    }

    bufSize = sizeof(buffer);
    ret = nvs_get_blob(nvsHandle, NVS_KEY_SESSION, buffer, &bufSize);
    if (ret == ESP_OK) {
        node.setBufferSession(buffer);
        ESP_LOGI(TAG, "[NVS] Session restored");
    }
}

static void printHex(const uint8_t* data, size_t len) {
    printf("[HEX]: ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

static void printAscii(const uint8_t* data, size_t len) {
    printf("[ASCII]: ");
    for (size_t i = 0; i < len; i++) {
        putchar((data[i] >= 32 && data[i] <= 126) ? data[i] : '.');
    }
    printf("\n");
}

extern "C" void app_main(void) {
    nvs_flash_init();
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);

    ESP_LOGI(TAG, "\n[Setup] Starting...");

    ESP_LOGI(TAG, "[Radio] Initializing...");
    int state = radio.begin();

    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Radio init failed, code: %d", state);
        halt();
    }

    radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

    state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Node init failed, code: %d", state);
        halt();
    }

    restoreLoRaWANState();

    ESP_LOGI(TAG, "[LoRaWAN] Joining network...");
    state = node.activateOTAA();

    const bool joinFailed = (state != RADIOLIB_LORAWAN_NEW_SESSION) &&
                            (state != RADIOLIB_LORAWAN_SESSION_RESTORED);
    if (joinFailed) {
        ESP_LOGE(TAG, "Join failed, code: %d", state);
        halt();
    }

    saveLoRaWANState();
    ESP_LOGI(TAG, "[LoRaWAN] Ready!\n");

    while (1) {
        ESP_LOGI(TAG, "[LoRaWAN] Sending uplink...");

        const uint8_t val1 = radio.random(100);
        const uint16_t val2 = radio.random(2000);
        const uint8_t uplinkPayload[3] = {val1, (uint8_t)(val2 >> 8), (uint8_t)(val2 & 0xFF)};

        uint8_t downlink[64];
        size_t downlinkLen = sizeof(downlink);

        const int16_t state = node.sendReceive(
            uplinkPayload,
            sizeof(uplinkPayload),
            1,
            downlink,
            &downlinkLen
        );

        if (state < RADIOLIB_ERR_NONE) {
            ESP_LOGE(TAG, "sendReceive error, code: %d", state);
        }

        if (state >= 0) {
            saveLoRaWANState();
        }

        if (state > 0) {
            ESP_LOGI(TAG, "[LoRaWAN] Downlink received");
            ESP_LOGI(TAG, "RX Window: %d", state);
            ESP_LOGI(TAG, "Length: %d", downlinkLen);
            printHex(downlink, downlinkLen);
            printAscii(downlink, downlinkLen);
        } else {
            ESP_LOGI(TAG, "[LoRaWAN] No downlink received");
        }

        ESP_LOGI(TAG, "[Timer] Next uplink in %d s\n", uplinkIntervalSeconds);
        hal->delay(30 * 1000);
    }
}
