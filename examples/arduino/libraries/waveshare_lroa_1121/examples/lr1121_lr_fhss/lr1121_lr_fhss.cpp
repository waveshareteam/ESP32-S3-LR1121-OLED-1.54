/*!
 * @file      lr1121_lr_fhss.cpp
 *
 * @brief     TX LR-FHSS example for LR11xx chip
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2023. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "lr1121_lr_fhss.h"

bool irq_flag; // Flag to indicate if an interrupt has occurred

static const uint8_t lr_fhss_sync_word[LR_FHSS_SYNC_WORD_BYTES] = { 0x2C, 0x0F, 0x79, 0x95 };

static lr11xx_lr_fhss_params_t lr_fhss_params;

static uint8_t buffer[PAYLOAD_LENGTH];

/*!
 * @brief Configure a LR-FHSS payload and send it, element 0 of the payload is incremented by one at each call
 *
 * @param [in] params Parameter configuration structure of the LR-FHSS
 * @param [in] payload The payload to send. It is the responsibility of the caller to ensure that this references an
 * array containing at least length elements
 * @param [in] length The length of the payload
 */
void build_frame_and_send( const lr11xx_lr_fhss_params_t* params, uint8_t* payload, uint16_t length )
{
    const uint16_t hop_seq_id = ( uint16_t )( rand( ) % lr11xx_lr_fhss_get_hop_sequence_count( params ) );

    lr11xx_lr_fhss_build_frame( &lr1121, &lr_fhss_params, hop_seq_id, payload, length );

    buffer[0]++;
    lr11xx_radio_set_tx( &lr1121, 0 );
}

void ARDUINO_ISR_ATTR isr() {
    irq_flag = true; // Set the interrupt flag
}

void lr1121_lr_fhss_test()
{
    lora_init_io_context(&lr1121); // Initialize the I/O context for the LR1121
    lora_init_io(&lr1121);         // Initialize the I/O for the LR1121

    lora_spi_init(&lr1121); // Initialize the SPI interface for the LR1121
    // Initialize the system
    lora_system_init(&lr1121);
    // Print the version number for verification
    lora_print_version(&lr1121);

    // Initialize the LoRa radio
    lora_radio_init(&lr1121);

    lora_init_irq(&lr1121, isr); // Initialize the interrupt service routine

    ASSERT_LR11XX_RC( lr11xx_system_set_dio_irq_params( &lr1121, LR11XX_SYSTEM_IRQ_TX_DONE, LR11XX_SYSTEM_IRQ_NONE ) );
    ASSERT_LR11XX_RC( lr11xx_system_clear_irq_status( &lr1121, LR11XX_SYSTEM_IRQ_ALL_MASK ) );


    for( int i = 0; i < PAYLOAD_LENGTH; i++ )
    {
        buffer[i] = i;
    }

     /* Init parameters */
    lr_fhss_params.device_offset                  = 0;
    lr_fhss_params.lr_fhss_params.bw              = LR_FHSS_BANDWIDTH;
    lr_fhss_params.lr_fhss_params.cr              = LR_FHSS_CODING_RATE;
    lr_fhss_params.lr_fhss_params.enable_hopping  = LR_FHSS_ENABLE_HOPPING;
    lr_fhss_params.lr_fhss_params.grid            = LR_FHSS_GRID;
    lr_fhss_params.lr_fhss_params.header_count    = LR_FHSS_HEADER_COUNT;
    lr_fhss_params.lr_fhss_params.modulation_type = LR_FHSS_MODULATION_TYPE;
    lr_fhss_params.lr_fhss_params.sync_word       = lr_fhss_sync_word;

    build_frame_and_send( &lr_fhss_params, buffer, PAYLOAD_LENGTH );

    while (1)
    {
        if (irq_flag)
            lora_irq_process(&lr1121,LR11XX_SYSTEM_IRQ_TX_DONE); // Process any pending interrupts                         // Increment the counter
        delay(1); // Short delay to control the loop speed
    }
}

void on_tx_done( void )
{
    delay( TX_TO_TX_DELAY_IN_MS );

    build_frame_and_send( &lr_fhss_params, buffer, PAYLOAD_LENGTH );
}

void lora_irq_process(const void *context, lr11xx_system_irq_mask_t irq_filter_mask)
{
    irq_flag = false; // Clear the interrupt flag
    lr11xx_system_irq_mask_t radio_irq = LR11XX_SYSTEM_IRQ_NONE;
    // Get the IRQ status
    if (lr11xx_system_get_irq_status(context, &radio_irq) != LR11XX_STATUS_OK)
    {
        printf("get_irq_status failed\r\n");
    }

    // Apply the IRQ filter mask to get the relevant IRQ status
    radio_irq &= irq_filter_mask;

    if ((radio_irq & LR11XX_SYSTEM_IRQ_TX_DONE) == LR11XX_SYSTEM_IRQ_TX_DONE)
    {
        // Clear the IRQ status
        lr11xx_system_clear_irq_status(context, radio_irq);
        printf("TX DONE\r\n");
        on_tx_done();
    }
}
