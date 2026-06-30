/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_state.h"

// Defaults are selected for a usable GFSK voice link at first boot.
bool Boot_Key_Flag = false;
bool Lora_Transmission_Mode = false;
volatile radio_modem_t radio_modem = RADIO_MODEM_GFSK;
volatile radio_band_t radio_band = RADIO_BAND_HF;
volatile bool radio_reconfigure_requested = false;
volatile float radio_frequency_mhz = 868.0f;
volatile uint8_t radio_lora_spreading_factor = 7;
volatile float radio_lora_bandwidth_khz = 125.0f;
volatile uint8_t radio_lora_coding_rate = 5;
volatile float radio_gfsk_bit_rate_kbps = 50.0f;
volatile float radio_gfsk_freq_dev_khz = 25.0f;
volatile float radio_gfsk_rx_bw_khz = 117.3f;
volatile int8_t radio_tx_power_dbm = 22;

pcf85063a_datetime_t time_info;
bsp_bat_info_t bat_info;
