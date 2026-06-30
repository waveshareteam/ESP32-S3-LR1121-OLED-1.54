/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "radio_settings.h"

#include <stdio.h>

#include "nvs.h"

// UI selection indexes mirror the roller positions and are saved to NVS.
uint8_t last_md = RADIO_MODEM_GFSK;
uint8_t last_band = RADIO_BAND_HF;
float last_freq_mhz = 868.0f;
uint8_t last_sf = 2;
uint8_t last_bw = 1;
uint8_t last_cr = 1;
uint8_t last_pwr = 6;
uint8_t last_gfsk_br = 1;
uint8_t last_gfsk_dev = 1;
uint8_t last_gfsk_rx = 1;

lora_config_t lora_config = {
    .frequency_hz = 868.0f,
    .tx_power_dbm = 22,
    .spreading_factor = 7,
    .bandwidth = 125.0f,
    .coding_rate = 5,
    .pa_ramp_time = 48,
};

// Roller option tables are the single source of truth for UI and radio values.
const uint8_t sf_values[] = {
    5, 6, 7, 8, 9, 10, 11, 12
};

const float bw_values[] = {
    62.5f, 125.0f, 250.0f, 500.0f
};

const float gfsk_br_values[] = {
    25.0f, 50.0f, 100.0f
};

const float gfsk_dev_values[] = {
    12.5f, 25.0f, 50.0f
};

const float gfsk_rx_bw_values[] = {
    58.6f, 117.3f, 234.3f
};

const int8_t tx_power_values[] = {
    0, 5, 10, 14, 17, 20, 22
};

const uint8_t SF_OPTION_COUNT = sizeof(sf_values) / sizeof(sf_values[0]);
const uint8_t BW_OPTION_COUNT = sizeof(bw_values) / sizeof(bw_values[0]);
const uint8_t GFSK_BR_OPTION_COUNT = sizeof(gfsk_br_values) / sizeof(gfsk_br_values[0]);
const uint8_t GFSK_DEV_OPTION_COUNT = sizeof(gfsk_dev_values) / sizeof(gfsk_dev_values[0]);
const uint8_t GFSK_RX_OPTION_COUNT = sizeof(gfsk_rx_bw_values) / sizeof(gfsk_rx_bw_values[0]);
const uint8_t PWR_OPTION_COUNT = sizeof(tx_power_values) / sizeof(tx_power_values[0]);
const uint8_t CR_OPTION_COUNT = 8;
static const uint8_t RADIO_HF_FREQ_STEP_MHZ = 2;
static const uint8_t RADIO_LF_FREQ_STEP_MHZ = 1;
static const uint16_t RADIO_HF_MIN_FREQ_MHZ = 862;
static const uint16_t RADIO_HF_MAX_FREQ_MHZ = 923;
static const uint16_t RADIO_HF_DEFAULT_FREQ_MHZ = 868;
static const uint16_t RADIO_LF_MIN_FREQ_MHZ = 410;
static const uint16_t RADIO_LF_MAX_FREQ_MHZ = 510;
static const uint16_t RADIO_LF_DEFAULT_FREQ_MHZ = 433;

const char LORA_SF_OPTIONS[] =
    "SF5\n"
    "SF6\n"
    "SF7\n"
    "SF8\n"
    "SF9\n"
    "SF10\n"
    "SF11\n"
    "SF12";

const char LORA_BW_OPTIONS[] =
    "62.50 kHz\n"
    "125 kHz\n"
    "250 kHz\n"
    "500 kHz";

const char LORA_CR_OPTIONS[] =
    "NO CR\n"
    "CR 45\n"
    "CR 46\n"
    "CR 47\n"
    "CR 48\n"
    "CR LI 45\n"
    "CR LI 46\n"
    "CR LI 48";

const char GFSK_BR_OPTIONS[] =
    "25 kbps\n"
    "50 kbps\n"
    "100 kbps";

const char GFSK_DEV_OPTIONS[] =
    "12.5 kHz\n"
    "25 kHz\n"
    "50 kHz";

const char GFSK_RX_OPTIONS[] =
    "58.6 kHz\n"
    "117.3 kHz\n"
    "234.3 kHz";

const char RADIO_BAND_OPTIONS[] =
    "HF\n"
    "LF";

static const char RADIO_HF_FREQ_OPTIONS[] =
    "862\n864\n866\n868\n870\n872\n874\n876\n878\n880\n"
    "882\n884\n886\n888\n890\n892\n894\n896\n898\n900\n"
    "902\n904\n906\n908\n910\n912\n914\n916\n918\n920\n"
    "922\n923";

static const char RADIO_LF_FREQ_OPTIONS[] =
    "410\n411\n412\n413\n414\n415\n416\n417\n418\n419\n"
    "420\n421\n422\n423\n424\n425\n426\n427\n428\n429\n"
    "430\n431\n432\n433\n434\n435\n436\n437\n438\n439\n"
    "440\n441\n442\n443\n444\n445\n446\n447\n448\n449\n"
    "450\n451\n452\n453\n454\n455\n456\n457\n458\n459\n"
    "460\n461\n462\n463\n464\n465\n466\n467\n468\n469\n"
    "470\n471\n472\n473\n474\n475\n476\n477\n478\n479\n"
    "480\n481\n482\n483\n484\n485\n486\n487\n488\n489\n"
    "490\n491\n492\n493\n494\n495\n496\n497\n498\n499\n"
    "500\n501\n502\n503\n504\n505\n506\n507\n508\n509\n"
    "510";

const char TX_POWER_OPTIONS[] =
    "0 dBm\n"
    "5 dBm\n"
    "10 dBm\n"
    "14 dBm\n"
    "17 dBm\n"
    "20 dBm\n"
    "22 dBm";

static const char *RADIO_NVS_NAMESPACE = "radio";

static uint16_t radio_band_min_freq(radio_band_t band)
{
    return band == RADIO_BAND_LF ? RADIO_LF_MIN_FREQ_MHZ : RADIO_HF_MIN_FREQ_MHZ;
}

static uint16_t radio_band_max_freq(radio_band_t band)
{
    return band == RADIO_BAND_LF ? RADIO_LF_MAX_FREQ_MHZ : RADIO_HF_MAX_FREQ_MHZ;
}

static uint16_t radio_band_default_freq(radio_band_t band)
{
    return band == RADIO_BAND_LF ? RADIO_LF_DEFAULT_FREQ_MHZ : RADIO_HF_DEFAULT_FREQ_MHZ;
}

static uint8_t radio_band_freq_step(radio_band_t band)
{
    return band == RADIO_BAND_LF ? RADIO_LF_FREQ_STEP_MHZ : RADIO_HF_FREQ_STEP_MHZ;
}

static bool radio_frequency_in_band(radio_band_t band, uint16_t freq_mhz)
{
    return freq_mhz >= radio_band_min_freq(band) &&
           freq_mhz <= radio_band_max_freq(band);
}

static uint8_t radio_band_freq_count(radio_band_t band)
{
    uint16_t min_freq = radio_band_min_freq(band);
    uint16_t max_freq = radio_band_max_freq(band);
    uint8_t step = radio_band_freq_step(band);
    return (uint8_t)(((max_freq - min_freq + step - 1) / step) + 1);
}

static void radio_apply_frequency(uint16_t freq_mhz)
{
    last_freq_mhz = (float)freq_mhz;
    radio_frequency_mhz = (float)freq_mhz;
    lora_config.frequency_hz = (float)freq_mhz;
}

const char *radio_frequency_options(void)
{
    return radio_band == RADIO_BAND_LF ? RADIO_LF_FREQ_OPTIONS : RADIO_HF_FREQ_OPTIONS;
}

const char *radio_band_label(void)
{
    return radio_band == RADIO_BAND_LF ? "LF" : "HF";
}

// Convert MHz to the frequency roller index while clamping to the supported band.
uint8_t radio_freq_to_index(float freq_mhz)
{
    uint16_t min_freq = radio_band_min_freq((radio_band_t)radio_band);
    uint16_t max_freq = radio_band_max_freq((radio_band_t)radio_band);
    int freq = (int)(freq_mhz + 0.5f);

    if (freq < min_freq) {
        return 0;
    }
    if (freq >= max_freq) {
        return radio_band_freq_count((radio_band_t)radio_band) - 1;
    }

    return (uint8_t)((freq - min_freq) / radio_band_freq_step((radio_band_t)radio_band));
}

float radio_index_to_freq(uint8_t index)
{
    uint16_t min_freq = radio_band_min_freq((radio_band_t)radio_band);
    uint16_t max_freq = radio_band_max_freq((radio_band_t)radio_band);
    uint16_t freq = min_freq + index * radio_band_freq_step((radio_band_t)radio_band);

    if (freq > max_freq) {
        freq = max_freq;
    }

    return (float)freq;
}

void radio_set_frequency_mhz(float freq_mhz)
{
    uint16_t freq = (uint16_t)(freq_mhz + 0.5f);
    if (!radio_frequency_in_band((radio_band_t)radio_band, freq)) {
        freq = radio_band_default_freq((radio_band_t)radio_band);
    }

    radio_apply_frequency((uint16_t)radio_index_to_freq(radio_freq_to_index((float)freq)));
}

void radio_select_band(radio_band_t band)
{
    radio_band_t next_band = band == RADIO_BAND_LF ? RADIO_BAND_LF : RADIO_BAND_HF;
    radio_band = next_band;
    last_band = (uint8_t)next_band;

    uint16_t freq = (uint16_t)(last_freq_mhz + 0.5f);
    if (!radio_frequency_in_band(next_band, freq)) {
        freq = radio_band_default_freq(next_band);
    }

    radio_set_frequency_mhz((float)freq);
}

const char *radio_format_bandwidth(float bandwidth)
{
    int bw_x10 = (int)(bandwidth * 10.0f + 0.5f);

    switch (bw_x10) {
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

void radio_format_decimal(char *out, size_t out_size, float value)
{
    int value_x10 = (int)(value * 10.0f + 0.5f);

    if ((value_x10 % 10) == 0) {
        snprintf(out, out_size, "%d", value_x10 / 10);
    } else {
        snprintf(out, out_size, "%d.%d", value_x10 / 10, value_x10 % 10);
    }
}

// Load only validated option indexes so corrupt NVS cannot create invalid RF settings.
void radio_settings_load(void)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(RADIO_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        return;
    }

    uint8_t value = 0;
    if (nvs_get_u8(nvs, "mode", &value) == ESP_OK && value <= RADIO_MODEM_LORA) {
        last_md = value;
        radio_modem = (value == RADIO_MODEM_GFSK) ? RADIO_MODEM_GFSK : RADIO_MODEM_LORA;
    }

    if (nvs_get_u8(nvs, "band", &value) == ESP_OK && value <= RADIO_BAND_LF) {
        radio_select_band((radio_band_t)value);
    } else {
        radio_select_band((radio_band_t)last_band);
    }

    uint16_t freq_mhz = 0;
    if (nvs_get_u16(nvs, "freq", &freq_mhz) == ESP_OK) {
        radio_set_frequency_mhz((float)freq_mhz);
    }

    if (nvs_get_u8(nvs, "sf", &value) == ESP_OK && value < SF_OPTION_COUNT) {
        last_sf = value;
        lora_config.spreading_factor = sf_values[value];
        radio_lora_spreading_factor = lora_config.spreading_factor;
    }

    if (nvs_get_u8(nvs, "bw", &value) == ESP_OK && value < BW_OPTION_COUNT) {
        last_bw = value;
        lora_config.bandwidth = bw_values[value];
        radio_lora_bandwidth_khz = lora_config.bandwidth;
    }

    if (nvs_get_u8(nvs, "cr", &value) == ESP_OK && value < CR_OPTION_COUNT) {
        last_cr = value;
        lora_config.coding_rate = value + 4;
        radio_lora_coding_rate = lora_config.coding_rate;
    }

    if (nvs_get_u8(nvs, "gbr", &value) == ESP_OK && value < GFSK_BR_OPTION_COUNT) {
        last_gfsk_br = value;
        radio_gfsk_bit_rate_kbps = gfsk_br_values[value];
    }

    if (nvs_get_u8(nvs, "gdev", &value) == ESP_OK && value < GFSK_DEV_OPTION_COUNT) {
        last_gfsk_dev = value;
        radio_gfsk_freq_dev_khz = gfsk_dev_values[value];
    }

    if (nvs_get_u8(nvs, "grx", &value) == ESP_OK && value < GFSK_RX_OPTION_COUNT) {
        last_gfsk_rx = value;
        radio_gfsk_rx_bw_khz = gfsk_rx_bw_values[value];
    }

    if (nvs_get_u8(nvs, "pwr", &value) == ESP_OK && value < PWR_OPTION_COUNT) {
        last_pwr = value;
        lora_config.tx_power_dbm = tx_power_values[value];
        radio_tx_power_dbm = lora_config.tx_power_dbm;
    }

    nvs_close(nvs);
}

// Save compact indexes instead of floats to keep NVS stable across menu text changes.
void radio_settings_save(void)
{
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(RADIO_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        return;
    }

    nvs_set_u8(nvs, "mode", last_md);
    nvs_set_u8(nvs, "band", last_band);
    nvs_set_u16(nvs, "freq", (uint16_t)(last_freq_mhz + 0.5f));
    nvs_set_u8(nvs, "sf", last_sf);
    nvs_set_u8(nvs, "bw", last_bw);
    nvs_set_u8(nvs, "cr", last_cr);
    nvs_set_u8(nvs, "gbr", last_gfsk_br);
    nvs_set_u8(nvs, "gdev", last_gfsk_dev);
    nvs_set_u8(nvs, "grx", last_gfsk_rx);
    nvs_set_u8(nvs, "pwr", last_pwr);
    nvs_commit(nvs);
    nvs_close(nvs);
}
