/*!
 * @file      lr1121_ping_pong.cpp
 *
 * @brief     Ping-pong example for LR11xx chip
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2022. All rights reserved.
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

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lr1121_ping_pong.h"

bool irq_flag = false; // Flag to indicate if an interrupt has occurred

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */
static uint8_t buffer_tx[PAYLOAD_LENGTH];
static bool    is_master = true;

static const uint8_t ping_msg[PING_PONG_PREFIX_SIZE] = "PING";
static const uint8_t pong_msg[PING_PONG_PREFIX_SIZE] = "PONG";

static uint8_t  iteration       = 0;
static uint16_t packets_to_sync = 0;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/**
 * @brief Handle reception failure for ping-pong example
 */
static void ping_pong_reception_failure_handling( void );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void ARDUINO_ISR_ATTR isr() {
    irq_flag = true; // Set the interrupt flag
}

/**
 * @brief Main application entry point.
 */
void lr1121_ping_pong_init( void )
{
    lora_init_io_context( &lr1121 ); // Initialize the I/O context for the LR1121
    lora_init_io(&lr1121);           // Initialize the I/O for the LR1121
    lora_spi_init(&lr1121);          // Initialize the SPI interface for the LR1121

    printf( "===== LR11xx Ping-Pong example =====\n\n" );
    printf( "LR11XX driver version: %s\n", lr11xx_driver_version_get_version_string( ) );

    lora_system_init( &lr1121 );
    lora_print_version( &lr1121 );
    lora_radio_init( &lr1121 );

    lora_init_irq(&lr1121, isr);     // Initialize the interrupt service routine


    ASSERT_LR11XX_RC( lr11xx_system_set_dio_irq_params( &lr1121, IRQ_MASK, 0 ) );
    ASSERT_LR11XX_RC( lr11xx_system_clear_irq_status( &lr1121, LR11XX_SYSTEM_IRQ_ALL_MASK ) );

    /* Intializes random number generator */
    srand( 10 );

    memcpy( buffer_tx, ping_msg, PING_PONG_PREFIX_SIZE );
    buffer_tx[PING_PONG_PREFIX_SIZE] = ( uint8_t ) 0;
    buffer_tx[ITERATION_INDEX]       = ( uint8_t ) ( iteration );
    for( int i = PING_PONG_PREFIX_SIZE + 1 + 1; i < PAYLOAD_LENGTH; i++ )
    {
        buffer_tx[i] = i;
    }

    ASSERT_LR11XX_RC( lr11xx_regmem_write_buffer8( &lr1121, buffer_tx, PAYLOAD_LENGTH ) );

    
    ASSERT_LR11XX_RC( lr11xx_radio_set_tx( &lr1121, 0 ) );
}

void on_tx_done( void )
{
    
    printf( "Sent message %s, iteration %d\n", buffer_tx, iteration );
    delay( DELAY_PING_PONG_PACE_MS );
    ASSERT_LR11XX_RC( lr11xx_radio_set_rx(
        &lr1121,
        get_time_on_air_in_ms( ) + RX_TIMEOUT_VALUE + rand( ) % 500 ) );  // Random delay to avoid
                                                                          // unwanted synchronization
}

void lora_receive( const void* context, uint8_t* buffer, uint8_t buffer_length, uint8_t* size )
{
    lr11xx_radio_rx_buffer_status_t rx_buffer_status;
    lr11xx_radio_pkt_status_lora_t  pkt_status_lora;
    lr11xx_radio_pkt_status_gfsk_t  pkt_status_gfsk;

    lr11xx_radio_get_rx_buffer_status( context, &rx_buffer_status );
    *size = rx_buffer_status.pld_len_in_bytes;
    if( *size > buffer_length )
    {
        printf( "Received payload (size: %d) is bigger than the buffer (size: %d)!\n", *size,
                             buffer_length );
        return;
    }
    lr11xx_regmem_read_buffer8( context, buffer, rx_buffer_status.buffer_start_pointer,
                                rx_buffer_status.pld_len_in_bytes );

    HAL_DBG_TRACE_ARRAY( "Packet content", buffer, *size );

    printf( "Packet status:\n" );
    if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_LORA )
    {
        lr11xx_radio_get_lora_pkt_status( context, &pkt_status_lora );
        printf( "  - RSSI packet = %i dBm\n", pkt_status_lora.rssi_pkt_in_dbm );
        printf( "  - Signal RSSI packet = %i dBm\n", pkt_status_lora.signal_rssi_pkt_in_dbm );
        printf( "  - SNR packet = %i dB\n", pkt_status_lora.snr_pkt_in_db );
    }
    else if( PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_GFSK )
    {
        lr11xx_radio_get_gfsk_pkt_status( context, &pkt_status_gfsk );
        printf( "  - RSSI average = %i dBm\n", pkt_status_gfsk.rssi_avg_in_dbm );
        printf( "  - RSSI sync = %i dBm\n", pkt_status_gfsk.rssi_sync_in_dbm );
    }
}

void on_rx_done( void )
{
    uint8_t buffer_rx[PAYLOAD_LENGTH];
    uint8_t size;

    packets_to_sync = 0;


    lora_receive( &lr1121, buffer_rx, PAYLOAD_LENGTH, &size );
    iteration = buffer_rx[ITERATION_INDEX];

    iteration++;
    printf( "Received message %s, iteration %d\n", buffer_rx, iteration );

    if( is_master == true )
    {
        if( memcmp( buffer_rx, ping_msg, PING_PONG_PREFIX_SIZE ) == 0 )
        {
            is_master = false;
            memcpy( buffer_tx, pong_msg, PING_PONG_PREFIX_SIZE );
        }
        else if( memcmp( buffer_rx, pong_msg, PING_PONG_PREFIX_SIZE ) != 0 )
        {
            printf( "Unexpected message\n" );
        }
    }
    else
    {
        if( memcmp( buffer_rx, ping_msg, PING_PONG_PREFIX_SIZE ) != 0 )
        {
            printf( "Unexpected message\n" );

            is_master = true;
            memcpy( buffer_tx, ping_msg, PING_PONG_PREFIX_SIZE );
        }
    }

    delay( DELAY_PING_PONG_PACE_MS + DELAY_BEFORE_TX_MS );
    buffer_tx[ITERATION_INDEX] = iteration;

    ASSERT_LR11XX_RC( lr11xx_regmem_write_buffer8( &lr1121, buffer_tx, PAYLOAD_LENGTH ) );

    ASSERT_LR11XX_RC( lr11xx_radio_set_tx( &lr1121, 0 ) );
}

void on_rx_timeout( void )
{
    packets_to_sync++;
    if( packets_to_sync > SYNC_PACKET_THRESHOLD )
    {
        printf(
            "It looks like synchronisation is still not done, consider resetting one of the board\n" );
    }
    ping_pong_reception_failure_handling( );
}

void on_rx_crc_error( void )
{
    ping_pong_reception_failure_handling( );
}

void on_fsk_len_error( void )
{
    ping_pong_reception_failure_handling( );
}

static void ping_pong_reception_failure_handling( void )
{
    

    is_master = true;
    iteration = 0;
    memcpy( buffer_tx, ping_msg, PING_PONG_PREFIX_SIZE );
    buffer_tx[ITERATION_INDEX] = iteration;

    ASSERT_LR11XX_RC( lr11xx_regmem_write_buffer8( &lr1121, buffer_tx, PAYLOAD_LENGTH ) );
    
    ASSERT_LR11XX_RC( lr11xx_radio_set_tx( &lr1121, 0 ) );
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


    if( ( irq_regs & LR11XX_SYSTEM_IRQ_HEADER_ERROR ) == LR11XX_SYSTEM_IRQ_HEADER_ERROR )
    {
        printf( "Header error\n" );
    }

    if( ( irq_regs & LR11XX_SYSTEM_IRQ_RX_DONE ) == LR11XX_SYSTEM_IRQ_RX_DONE )
    {
        if( ( irq_regs & LR11XX_SYSTEM_IRQ_CRC_ERROR ) == LR11XX_SYSTEM_IRQ_CRC_ERROR )
        {
            printf( "CRC error\n" );
            on_rx_crc_error( );
        }
        else if( ( irq_regs & LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR ) == LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR )
        {
            printf( "FSK length error\n" );
            on_fsk_len_error( );
        }
        else
        {
            printf( "Rx done\n" );
            on_rx_done( );
        }
    }

    if( ( irq_regs & LR11XX_SYSTEM_IRQ_TIMEOUT ) == LR11XX_SYSTEM_IRQ_TIMEOUT )
    {
        printf( "Rx timeout\n" );
        on_rx_timeout( );
    }

    printf( "\n" );

}