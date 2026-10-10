/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "EspHal.h"
#include "config.h"
#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"

static EspHal hal;
static Module radio_module(&hal, BSP_SPI_CS, BSP_SPI_INT, BSP_SPI_RESET, BSP_SPI_BUSY);
static LR1121 radio(&radio_module);

static LoRaWANNode node(&radio, &lorawan_region, lorawan_sub_band);

static const char *TAG = "lorawan_starter";

#include "board_radio.h"

static nvs_handle_t lorawan_nvs_handle;

static const char *NVS_NAMESPACE = "lorawan";
static const char *NVS_KEY_DEV_NONCE = "nonce";
static const char *NVS_KEY_SESSION = "sess";

static void halt()
{
    while (true) {
        hal.delay(10);
    }
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Failed to erase NVS");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to initialize NVS");

    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, &lorawan_nvs_handle);
}

static esp_err_t save_lorawan_state(void)
{
    ESP_RETURN_ON_ERROR(nvs_set_blob(lorawan_nvs_handle, NVS_KEY_DEV_NONCE,
                                     node.getBufferNonces(),
                                     RADIOLIB_LORAWAN_NONCES_BUF_SIZE),
                        TAG, "Failed to save LoRaWAN nonce state");

    ESP_RETURN_ON_ERROR(nvs_set_blob(lorawan_nvs_handle, NVS_KEY_SESSION,
                                     node.getBufferSession(),
                                     RADIOLIB_LORAWAN_SESSION_BUF_SIZE),
                        TAG, "Failed to save LoRaWAN session state");

    ESP_RETURN_ON_ERROR(nvs_commit(lorawan_nvs_handle), TAG, "Failed to commit LoRaWAN state");
    ESP_LOGI(TAG, "[NVS] State saved");
    return ESP_OK;
}

static void restore_lorawan_state(void)
{
    uint8_t nonce_buffer[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    size_t buf_size = sizeof(nonce_buffer);

    esp_err_t ret = nvs_get_blob(lorawan_nvs_handle, NVS_KEY_DEV_NONCE, nonce_buffer, &buf_size);
    if (ret == ESP_OK && buf_size == sizeof(nonce_buffer)) {
        node.setBufferNonces(nonce_buffer);
        ESP_LOGI(TAG, "[NVS] Nonce restored");
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to restore nonce state: %s", esp_err_to_name(ret));
    }

    uint8_t session_buffer[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
    buf_size = sizeof(session_buffer);
    ret = nvs_get_blob(lorawan_nvs_handle, NVS_KEY_SESSION, session_buffer, &buf_size);
    if (ret == ESP_OK && buf_size == sizeof(session_buffer)) {
        node.setBufferSession(session_buffer);
        ESP_LOGI(TAG, "[NVS] Session restored");
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to restore session state: %s", esp_err_to_name(ret));
    }
}

static void log_hex(const uint8_t *data, size_t len)
{
    ESP_LOG_BUFFER_HEX(TAG, data, len);
}

static void log_ascii(const uint8_t *data, size_t len)
{
    char ascii[65];
    size_t ascii_len = (len < (sizeof(ascii) - 1)) ? len : (sizeof(ascii) - 1);
    for (size_t i = 0; i < ascii_len; i++) {
        ascii[i] = (data[i] >= 32 && data[i] <= 126) ? (char)data[i] : '.';
    }
    ascii[ascii_len] = '\0';
    ESP_LOGI(TAG, "[ASCII]: %s", ascii);
}

extern "C" void app_main(void) {
    esp_err_t ret = init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        halt();
    }

    ESP_LOGI(TAG, "\n[Setup] Starting...");

    ESP_LOGI(TAG, "[Radio] Initializing...");
    int state = radio.begin();

    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Radio init failed, code: %d", state);
        halt();
    }

    radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

    state = node.beginOTAA(join_eui, dev_eui, nwk_key, app_key);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Node init failed, code: %d", state);
        halt();
    }

    restore_lorawan_state();

    ESP_LOGI(TAG, "[LoRaWAN] Joining network...");
    state = node.activateOTAA();

    const bool joinFailed = (state != RADIOLIB_LORAWAN_NEW_SESSION) &&
                            (state != RADIOLIB_LORAWAN_SESSION_RESTORED);
    if (joinFailed) {
        ESP_LOGE(TAG, "Join failed, code: %d", state);
        halt();
    }

    ret = save_lorawan_state();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save LoRaWAN state: %s", esp_err_to_name(ret));
    }
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
            ret = save_lorawan_state();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save LoRaWAN state: %s", esp_err_to_name(ret));
            }
        }

        if (state > 0) {
            ESP_LOGI(TAG, "[LoRaWAN] Downlink received");
            ESP_LOGI(TAG, "RX Window: %d", state);
            ESP_LOGI(TAG, "Length: %zu", downlinkLen);
            log_hex(downlink, downlinkLen);
            log_ascii(downlink, downlinkLen);
        } else {
            ESP_LOGI(TAG, "[LoRaWAN] No downlink received");
        }

        ESP_LOGI(TAG, "[Timer] Next uplink in %" PRIu32 " s\n", uplink_interval_seconds);
        hal.delay(uplink_interval_seconds * 1000UL);
    }
}
