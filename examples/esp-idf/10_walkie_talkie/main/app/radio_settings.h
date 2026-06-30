/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RADIO_SETTINGS_H
#define RADIO_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Runtime LoRa settings mirrored from the menu and persisted in NVS.
typedef struct {
    float frequency_hz;
    int8_t tx_power_dbm;
    uint8_t spreading_factor;
    float bandwidth;
    uint8_t coding_rate;
    uint32_t pa_ramp_time;
} lora_config_t;

extern lora_config_t lora_config;

extern uint8_t last_md;
extern uint8_t last_band;
extern float last_freq_mhz;
extern uint8_t last_sf;
extern uint8_t last_bw;
extern uint8_t last_cr;
extern uint8_t last_pwr;
extern uint8_t last_gfsk_br;
extern uint8_t last_gfsk_dev;
extern uint8_t last_gfsk_rx;

extern const uint8_t sf_values[];
extern const float bw_values[];
extern const float gfsk_br_values[];
extern const float gfsk_dev_values[];
extern const float gfsk_rx_bw_values[];
extern const int8_t tx_power_values[];

extern const uint8_t SF_OPTION_COUNT;
extern const uint8_t BW_OPTION_COUNT;
extern const uint8_t GFSK_BR_OPTION_COUNT;
extern const uint8_t GFSK_DEV_OPTION_COUNT;
extern const uint8_t GFSK_RX_OPTION_COUNT;
extern const uint8_t PWR_OPTION_COUNT;
extern const uint8_t CR_OPTION_COUNT;

extern const char RADIO_BAND_OPTIONS[];
extern const char LORA_SF_OPTIONS[];
extern const char LORA_BW_OPTIONS[];
extern const char LORA_CR_OPTIONS[];
extern const char GFSK_BR_OPTIONS[];
extern const char GFSK_DEV_OPTIONS[];
extern const char GFSK_RX_OPTIONS[];
extern const char TX_POWER_OPTIONS[];

const char *radio_frequency_options(void);
const char *radio_band_label(void);
uint8_t radio_freq_to_index(float freq_mhz);
float radio_index_to_freq(uint8_t index);
void radio_select_band(radio_band_t band);
void radio_set_frequency_mhz(float freq_mhz);
void radio_format_decimal(char *out, size_t out_size, float value);
const char *radio_format_bandwidth(float bandwidth);
void radio_settings_load(void);
void radio_settings_save(void);

#ifdef __cplusplus
}
#endif

#endif
