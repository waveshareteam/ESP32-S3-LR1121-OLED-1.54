/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "board_support.h"

#ifdef __cplusplus
extern "C" {
#endif

// Application-wide state shared by UI, radio runtime and audio tasks.
typedef enum {
    RADIO_MODEM_GFSK = 0,
    RADIO_MODEM_LORA = 1,
} radio_modem_t;

typedef enum {
    RADIO_BAND_HF = 0,
    RADIO_BAND_LF = 1,
} radio_band_t;

extern bool Boot_Key_Flag;
extern bool Lora_Transmission_Mode;
extern volatile radio_modem_t radio_modem;
extern volatile radio_band_t radio_band;
extern volatile bool radio_reconfigure_requested;
extern volatile float radio_frequency_mhz;
extern volatile uint8_t radio_lora_spreading_factor;
extern volatile float radio_lora_bandwidth_khz;
extern volatile uint8_t radio_lora_coding_rate;
extern volatile float radio_gfsk_bit_rate_kbps;
extern volatile float radio_gfsk_freq_dev_khz;
extern volatile float radio_gfsk_rx_bw_khz;
extern volatile int8_t radio_tx_power_dbm;

extern pcf85063a_datetime_t time_info;
extern bsp_bat_info_t bat_info;

#ifdef __cplusplus
}
#endif

#endif
