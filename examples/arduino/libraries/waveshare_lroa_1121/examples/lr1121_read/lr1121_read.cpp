/*!
 * @file      lr1121_read.cpp
 *
 * @brief     LR1121 Example for Reading Data
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

#include "lr1121_read.h"

bool irq_flag = false; // Flag to indicate if an interrupt has occurred
/* Receive data buffer */
uint8_t rx_buffer[255] = {0}; // Buffer to store received data
uint8_t rx_buffer_lenght = 0; // Length of the received data

/* RF parameters configuration */
lr11xx_radio_pkt_params_lora_t radio_pkt_params = {
    .preamble_len_in_symb = LORA_PREAMBLE_LENGTH, // Preamble length in symbols
    .header_type          = LORA_PKT_LEN_MODE,    // Header type (explicit or implicit)
    .pld_len_in_bytes     = PAYLOAD_LENGTH,       // Payload length in bytes
    .crc                  = LR11XX_RADIO_LORA_CRC_OFF, // CRC mode
    .iq                   = LORA_IQ,              // IQ inversion
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

void ARDUINO_ISR_ATTR isr() {
    irq_flag = true; // Set the interrupt flag
}

// Check if the received data is a string or an array
void check_data_type(uint8_t *data, size_t length) {
    // Check if it is a string
    int is_string = 1;
    for (size_t i = 0; i < length; i++) {
        // If it contains non-ASCII characters, it is not a string
        if (data[i] < 0x20 || data[i] > 0x7E) {
            is_string = 0;
            break;
        }
    }

    if (is_string) {
        // If it is a string, print the string content
        data[length] = '\0'; // Ensure the string ends with a null character
        printf("Received string: %s\n", data);
    } else {
        // If it is not a string, print the array content
        printf("Received array: ");
        for (size_t i = 0; i < length; i++) {
            printf("%02X ", data[i]); // Print each byte in hexadecimal format
        }
        printf("\n");
    }
}

/**
 * @brief Enter receive mode for the RF
 * @param None
 * @retval None
 */
void lora_rx(const void *context)
{
  if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_LORA )
  {
    // Configure the RF packet length for LoRa
    radio_pkt_params.pld_len_in_bytes = 255;
    // Set the RF packet parameters for LoRa
    lr11xx_radio_set_lora_pkt_params( context, &radio_pkt_params );
  }
  else if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_GFSK )
  {
    gfsk_pkt_params.pld_len_in_bytes = 255;

    lr11xx_radio_set_gfsk_pkt_params( context, &gfsk_pkt_params );
  }
  // Configure DIO9 interrupt for the LR1121
  lr11xx_system_set_dio_irq_params(context, IRQ_MASK, LR11XX_SYSTEM_IRQ_NONE);
  // Clear the DIO9 interrupt status
  lr11xx_system_clear_irq_status(context, LR11XX_SYSTEM_IRQ_ALL_MASK);
  // Enter receive mode
  lr11xx_radio_set_rx(context, 0);
}

void lora_read_init()
{
    lora_init_io_context( &lr1121 ); // Initialize the I/O context for the LR1121
    lora_init_io(&lr1121);           // Initialize the I/O for the LR1121
    
    lora_spi_init(&lr1121);          // Initialize the SPI interface for the LR1121
    // Initialize the system
    lora_system_init(&lr1121);
    // Verify the version number
    lora_print_version( &lr1121 );
    // Initialize the LoRa radio
    lora_radio_init( &lr1121 );
  
    lora_init_irq(&lr1121, isr);     // Initialize the interrupt service routine
    lora_rx(&lr1121);                // Enter receive mode
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


    // Check for timeout
    if ((radio_irq & LR11XX_SYSTEM_IRQ_TIMEOUT) == LR11XX_SYSTEM_IRQ_TIMEOUT)
    {
        // Clear the IRQ status
        lr11xx_system_clear_irq_status(context, radio_irq);
        // Enter receive mode
        lora_rx(context);
    }
    else if (radio_irq & LR11XX_SYSTEM_IRQ_HEADER_ERROR)
    {
        printf("header_error\r\n");
        lr11xx_system_clear_irq_status(context, radio_irq);
        lora_rx(context);
    }
    else if (radio_irq & LR11XX_SYSTEM_IRQ_CRC_ERROR)
    {
        printf("crc_error\r\n", sizeof("crc_error\r\n"), 100);
        lr11xx_system_clear_irq_status(context, radio_irq);
        lora_rx(context);
    }
    // Check if reception is complete
    else if ((radio_irq & LR11XX_SYSTEM_IRQ_RX_DONE) == LR11XX_SYSTEM_IRQ_RX_DONE)
    {
        if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_LORA )
        {
          lr11xx_radio_rx_buffer_status_t rx_buffer_status;
          // Get the received packet status and parameters
          lr11xx_radio_get_rx_buffer_status(context, &rx_buffer_status);
          rx_buffer_lenght = rx_buffer_status.pld_len_in_bytes;
          // Read the received data from the FIFO into the rx_buffer
          lr11xx_regmem_read_buffer8(context, rx_buffer,
                                    rx_buffer_status.buffer_start_pointer,
                                    rx_buffer_status.pld_len_in_bytes);
        }
        else if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_GFSK )
        {
          lr11xx_radio_rx_buffer_status_t rx_buffer_status;
          // Get the received packet status and parameters
          lr11xx_radio_get_rx_buffer_status(context, &rx_buffer_status);
          rx_buffer_lenght = rx_buffer_status.pld_len_in_bytes;
          // Read the received data from the FIFO into the rx_buffer
          lr11xx_regmem_read_buffer8(context, rx_buffer,
                                    rx_buffer_status.buffer_start_pointer,
                                    rx_buffer_status.pld_len_in_bytes);
        }
        
        // Print the received data
        check_data_type(rx_buffer,rx_buffer_lenght - 1);
        memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear the buffer

        // Clear the IRQ status
        lr11xx_system_clear_irq_status(context, radio_irq);
        // Enter receive mode
        lora_rx(context);
    }
    // Handle other IRQ events
    else
    {
        lora_rx(context);
    }
}