/*!
 * @file      lr1121_sigfox.ino
 *
 * @brief     Sigfox PHY example for LR11xx chip
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

#include "lr1121_config.h"

/*!
 *  @brief Delay in ms between the end of a transmission and the beginning of the next one
 */
#ifndef TX_TO_TX_DELAY_IN_MS
#define TX_TO_TX_DELAY_IN_MS 200
#endif

// Application payload = 0x01
const uint8_t sample0[] = { 0xaa, 0xaa, 0xa0, 0x8d, 0x01, 0x05, 0x98, 0xba, 0xdc, 0xfe, 0x01, 0x9a, 0x09, 0xfe, 0x04 };

bool irq_flag = false; // Flag to indicate if an interrupt has occurred

#define SIGFOX_PAYLOAD_LENGTH 15

static void send_frame( void );
void lora_irq_process( const void* context, lr11xx_system_irq_mask_t irq_filter_mask );

void ARDUINO_ISR_ATTR isr() {
    irq_flag = true; // Set the interrupt flag
}

void setup() 
{
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  delay(1000);          // Wait for 1 second to ensure the system stabilizes
  
  lora_init_io_context( &lr1121 ); // Initialize the I/O context for the LR1121
  lora_init_io(&lr1121);           // Initialize the I/O for the LR1121
  lora_spi_init(&lr1121);          // Initialize the SPI interface for the LR1121

  printf( "===== LR11xx Sigfox PHY example =====\n\n" );
  printf( "LR11XX driver version: %s\n", lr11xx_driver_version_get_version_string( ) );

  // Initialize the system
  lora_system_init(&lr1121);
  lora_print_version( &lr1121 );
  // Initialize the LoRa radio
  lora_radio_dbpsk_init( &lr1121, SIGFOX_PAYLOAD_LENGTH );

  lora_init_irq(&lr1121, isr);     // Initialize the interrupt service routine

  ASSERT_LR11XX_RC( lr11xx_system_set_dio_irq_params( &lr1121, LR11XX_SYSTEM_IRQ_TX_DONE, 0 ) );
  ASSERT_LR11XX_RC( lr11xx_system_clear_irq_status( &lr1121, LR11XX_SYSTEM_IRQ_ALL_MASK ) );

  send_frame( );
}

void loop() {

  if(irq_flag)
      lora_irq_process( &lr1121, LR11XX_SYSTEM_IRQ_TX_DONE );
  delay(1); // Short delay to control the loop speed

}

void lora_dbpsk_encode_buffer( const uint8_t* data_in, int bpsk_pld_len_in_bits, uint8_t* data_out )
{
    uint8_t in_byte;
    uint8_t out_byte;

    int data_in_bytecount = bpsk_pld_len_in_bits >> 3;
    in_byte               = *data_in++;

    uint8_t current = 0;

    // Process full bytes
    while( --data_in_bytecount >= 0 )
    {
        for( int i = 0; i < 8; ++i )
        {
            out_byte = ( out_byte << 1 ) | current;
            if( ( in_byte & 0x80 ) == 0 )
            {
                current = current ^ 0x01;
            }
            in_byte <<= 1;
        }
        in_byte     = *data_in++;
        *data_out++ = out_byte;
    }

    // Process remaining bits
    for( int i = 0; i < ( bpsk_pld_len_in_bits & 7 ); ++i )
    {
        out_byte = ( out_byte << 1 ) | current;
        if( ( in_byte & 0x80 ) == 0 )
        {
            current = current ^ 0x01;
        }
        in_byte <<= 1;
    }

    // Process last data bit
    out_byte = ( out_byte << 1 ) | current;
    if( ( bpsk_pld_len_in_bits & 7 ) == 7 )
    {
        *data_out++ = out_byte;
    }

    // Add duplicate bit and store
    out_byte  = ( out_byte << 1 ) | current;
    *data_out = out_byte << ( 7 - ( ( bpsk_pld_len_in_bits + 1 ) & 7 ) );
}

/*!
 * \brief Given the length of a BPSK frame, in bits, calculate the space necessary to hold the frame after differential
 * encoding, in bits.
 *
 * \param [in]  bpsk_pld_len_in_bits Length of a BPSK frame, in bits
 * \returns                          Space required for DBPSK frame, after addition of start/stop bits [bits]
 */
static inline int lora_dbpsk_get_pld_len_in_bits( int bpsk_pld_len_in_bits )
{
    // Hold the last bit one extra bit-time
    return bpsk_pld_len_in_bits + 2;
}

/*!
 * \brief Given the length of a BPSK frame, in bits, calculate the space necessary to hold the frame after differential
 * encoding, in bytes.
 *
 * \param [in]  bpsk_pld_len_in_bits Length of a BPSK frame, in bits
 * \returns                          Space required for DBPSK frame, after addition of start/stop bits [bytes]
 */
static inline int lora_dbpsk_get_pld_len_in_bytes( int bpsk_pld_len_in_bits )
{
    return ( lora_dbpsk_get_pld_len_in_bits( bpsk_pld_len_in_bits ) + 7 ) >> 3;
}

void send_frame( void )
{
    uint8_t frame_buffer[lora_dbpsk_get_pld_len_in_bytes( SIGFOX_PAYLOAD_LENGTH << 3 )];

    lora_dbpsk_encode_buffer( sample0, SIGFOX_PAYLOAD_LENGTH << 3, frame_buffer );

    lr11xx_regmem_write_buffer8( &lr1121, frame_buffer, lora_dbpsk_get_pld_len_in_bytes( SIGFOX_PAYLOAD_LENGTH << 3 ) );

    lr11xx_radio_set_tx( &lr1121, 0 );
}

void on_tx_done( void )
{


    delay( TX_TO_TX_DELAY_IN_MS );

    send_frame( );
}

void lora_irq_process( const void* context, lr11xx_system_irq_mask_t irq_filter_mask )
{

    irq_flag = false;

    lr11xx_system_irq_mask_t irq_regs;
    lr11xx_system_get_and_clear_irq_status( context, &irq_regs );

    printf( "Interrupt flags = 0x%08X\n", irq_regs );

    irq_regs &= irq_filter_mask;

    printf( "Interrupt flags (after filtering) = 0x%08X\n", irq_regs );

    if( ( irq_regs & LR11XX_SYSTEM_IRQ_TX_DONE ) == LR11XX_SYSTEM_IRQ_TX_DONE )
    {
        printf( "Tx done\n" );
        on_tx_done( );
    }

    printf( "\n" );
    
}

