/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "radio_runtime.h"

#include "app_state.h"
#include "radio_device.h"

#define RADIO_SYNC_WORD 0x12
#define RADIO_PREAMBLE_LENGTH 8
#define RADIO_TCXO_VOLTAGE 3.0f
#define RADIO_GFSK_PREAMBLE_LENGTH 32

volatile bool radio_irq_fired = false;

// RF switch pins select the correct LR1121 RF path for RX/TX modes.
#include "board_radio.h"

static void radio_irq_callback(void)
{
    radio_irq_fired = true;
}

void radio_restore_irq_action(void)
{
    radio.setIrqAction(radio_irq_callback);
}

int radio_apply_current_config(bool start_receive)
{
    int state = RADIOLIB_ERR_NONE;

    // Reapply the switch table after each modem rebuild.
    radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

    // GFSK parameters are applied through beginGFSK, so changes rebuild the modem.
    if (radio_modem == RADIO_MODEM_GFSK) {
        state = radio.beginGFSK((float)radio_frequency_mhz,
                                (float)radio_gfsk_bit_rate_kbps,
                                (float)radio_gfsk_freq_dev_khz,
                                (float)radio_gfsk_rx_bw_khz,
                                (int8_t)radio_tx_power_dbm,
                                RADIO_GFSK_PREAMBLE_LENGTH,
                                RADIO_TCXO_VOLTAGE);
        if (state != RADIOLIB_ERR_NONE) {
            return state;
        }

        // Both peers must use the same sync word or packets will be ignored.
        uint8_t gfsk_sync_word[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };
        state = radio.setSyncWord(gfsk_sync_word, sizeof(gfsk_sync_word));
        if (state != RADIOLIB_ERR_NONE) {
            return state;
        }
    } else {
        // LoRa can run narrower and farther, but packet airtime grows quickly with SF.
        state = radio.begin((float)radio_frequency_mhz,
                            (float)radio_lora_bandwidth_khz,
                            (uint8_t)radio_lora_spreading_factor,
                            (uint8_t)radio_lora_coding_rate,
                            RADIO_SYNC_WORD,
                            (int8_t)radio_tx_power_dbm,
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

    // The main loop completes TX/RX work outside the interrupt context.
    radio_restore_irq_action();
    return start_receive ? radio.startReceive() : radio.standby();
}
