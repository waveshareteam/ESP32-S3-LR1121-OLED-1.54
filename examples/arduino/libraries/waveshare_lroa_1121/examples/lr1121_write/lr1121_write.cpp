/*!
 * @file      lr1121_write.cpp
 *
 * @brief     LR1121 Example for Sending Data
 *
 * The Clear BSD License
 * Copyright Waveshare team 2025. All rights reserved.
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

#include "lr1121_write.h"

bool irq_flag; // Flag to indicate if an interrupt has occurred

/* RF parameters configuration */
lr11xx_radio_pkt_params_lora_t  radio_pkt_params = 
{
    .preamble_len_in_symb = LORA_PREAMBLE_LENGTH, // Preamble length in symbols
    .header_type = LORA_PKT_LEN_MODE,             // Header type (explicit or implicit)
    .pld_len_in_bytes = PAYLOAD_LENGTH,           // Payload length in bytes
    .crc = LR11XX_RADIO_LORA_CRC_OFF,             // CRC mode
    .iq = LORA_IQ                              // IQ inversion
};

lr11xx_radio_pkt_params_gfsk_t gfsk_pkt_params = {
    .preamble_len_in_bits  = FSK_PREAMBLE_LENGTH, // Preamble length in bits
    .preamble_detector     = FSK_PREAMBLE_DETECTOR, // Preamble detector type
    .sync_word_len_in_bits = FSK_SYNCWORD_LENGTH, // Syncword length in bits
    .address_filtering     = FSK_ADDRESS_FILTERING, // Address filtering mode
    .header_type           = FSK_HEADER_TYPE,       // Header type
    .pld_len_in_bytes      = PAYLOAD_LENGTH,        // Payload length in bytes
    .crc_type              = FSK_CRC_TYPE,          // CRC type
    .dc_free               = FSK_DC_FREE,           // DC-free encoding mode
};

/* Data buffer for automatic transmission */
uint8_t tx_buffer[] = {0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9};
	
void ARDUINO_ISR_ATTR isr() {
    irq_flag = true; // Set the interrupt flag
}

void lora_write_init()
{
    lora_init_io_context( &lr1121 ); // Initialize the I/O context for the LR1121
    lora_init_io(&lr1121);           // Initialize the I/O for the LR1121
    
    lora_spi_init(&lr1121);          // Initialize the SPI interface for the LR1121
    // lr11xx_hal_wakeup(&lr1121);   // Wake up the LR1121 (if needed)
    // Initialize the system
    lora_system_init(&lr1121);
    // Print the version number for verification
    lora_print_version( &lr1121 );
    // Initialize the LoRa radio
    lora_radio_init( &lr1121 );

    lora_init_irq(&lr1121, isr);     // Initialize the interrupt service routine
}

/**
 * @brief Transmit data using the RF module
 * @param None
 * @retval None
 */
void radio_tx_auto(const void* context)
{
    // Configure the DIO9 interrupt for TX done
    lr11xx_system_set_dio_irq_params(context, LR11XX_SYSTEM_IRQ_TX_DONE, LR11XX_SYSTEM_IRQ_NONE);
    // Clear the DIO9 interrupt status
    lr11xx_system_clear_irq_status( context, LR11XX_SYSTEM_IRQ_ALL_MASK);
    if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_LORA )
    {
        // Set the payload length for LoRa
        radio_pkt_params.pld_len_in_bytes = sizeof(tx_buffer);
        // Configure the LoRa packet parameters
        lr11xx_radio_set_lora_pkt_params( context, &radio_pkt_params );
        // Write the data to the LR1121 FIFO
        lr11xx_regmem_write_buffer8( context , tx_buffer, sizeof(tx_buffer));
        // Start transmission
        lr11xx_radio_set_tx(context,0);
    }
    else if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_GFSK )
    {
        // Set the payload length for GFSK
        gfsk_pkt_params.pld_len_in_bytes = sizeof(tx_buffer);
        // Configure the GFSK packet parameters
        lr11xx_radio_set_gfsk_pkt_params( context, &gfsk_pkt_params );

        // Write the data to the LR1121 FIFO
        lr11xx_regmem_write_buffer8( context , tx_buffer, sizeof(tx_buffer));
        // Start transmission
        lr11xx_radio_set_tx(context,0);
    }
}

/**
 * @brief Custom transmit API for the RF module
 * @param uint8_t *buffer: Pointer to the data to be transmitted
 * @param uint8_t lenght: Length of the data
 * @retval None
 */
void radio_tx_custom(const void* context, uint8_t *buffer, uint8_t lenght)
{
    if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_LORA )
    {
        // Configure the DIO9 interrupt for TX done
        lr11xx_system_set_dio_irq_params(context,LR11XX_SYSTEM_IRQ_TX_DONE, LR11XX_SYSTEM_IRQ_NONE);
        // Clear the DIO9 interrupt status
        lr11xx_system_clear_irq_status( context,  LR11XX_SYSTEM_IRQ_ALL_MASK);
        // Set the payload length for LoRa
        radio_pkt_params.pld_len_in_bytes = lenght;
        // Configure the LoRa packet parameters
        lr11xx_radio_set_lora_pkt_params( context, &radio_pkt_params );
        // Write the data to the LR1121 FIFO
        lr11xx_regmem_write_buffer8( context , buffer, lenght);
        // Start transmission
        lr11xx_radio_set_tx(context,0);
    }
    else if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_GFSK )
    {
        // Configure the DIO9 interrupt for TX done
        lr11xx_system_set_dio_irq_params(context,LR11XX_SYSTEM_IRQ_TX_DONE, LR11XX_SYSTEM_IRQ_NONE);
        // Clear the DIO9 interrupt status
        lr11xx_system_clear_irq_status( context,  LR11XX_SYSTEM_IRQ_ALL_MASK);

        // Set the payload length for GFSK
        gfsk_pkt_params.pld_len_in_bytes = lenght;
        // Configure the GFSK packet parameters
        lr11xx_radio_set_gfsk_pkt_params( context, &gfsk_pkt_params );

        // Write the data to the LR1121 FIFO
        lr11xx_regmem_write_buffer8( context , buffer, lenght);
        // Start transmission
        lr11xx_radio_set_tx(context,0);
    }
}

void lora_irq_process( const void* context )
{
    irq_flag = false; // Clear the interrupt flag
    lr11xx_system_irq_mask_t radio_irq = LR11XX_SYSTEM_IRQ_NONE;
    // Get the IRQ status
    if(lr11xx_system_get_irq_status(context ,&radio_irq) != LR11XX_STATUS_OK )
    {
        printf("get_irq_status failed\r\n");
    }
    // Check for timeout
    if(( radio_irq & LR11XX_SYSTEM_IRQ_TIMEOUT ) == LR11XX_SYSTEM_IRQ_TIMEOUT)
    {
        // Clear the IRQ status
        lr11xx_system_clear_irq_status( context, radio_irq );  
    }
    else if(radio_irq & LR11XX_SYSTEM_IRQ_HEADER_ERROR)
    {
        printf("header_error\r\n");
    }
    else if(radio_irq & LR11XX_SYSTEM_IRQ_CRC_ERROR)
    {
        printf("crc_error\r\n");
    }
    // Check if transmission is complete
    else if((radio_irq & LR11XX_SYSTEM_IRQ_TX_DONE) == LR11XX_SYSTEM_IRQ_TX_DONE)
    {
        // Clear the IRQ status
        lr11xx_system_clear_irq_status( context, radio_irq );  
        printf("TX DONE\r\n");
    }
    // Handle other IRQ events
    else
    {
        printf("Other IRQ event\r\n");
    }
}