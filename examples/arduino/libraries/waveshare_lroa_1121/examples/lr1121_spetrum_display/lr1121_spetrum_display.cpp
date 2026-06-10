/*!
 * @file      lr1121_spetrum_display.cpp
 *
 * @brief     Spectrum-display example for LR11xx chip
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

#include <stdlib.h>
#include "lr1121_spetrum_display.h"
#include "curve_plot.h"
/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/**
 * @brief Duration of the wait between setting to RX mode and valid instant RSSI value available
 *
 * Expressed in milliseconds
 *
 * @warning If switching from StandbyRC mode this delay is recommended to set to 30ms; if switching from StandbyXOSC,
 * 1ms.
 */
#define DELAY_BETWEEN_SET_RX_AND_VALID_RSSI_MS ( 1 )

/**
 * @brief Duration of the wait between each instant RSSI fetch. This is to make sure that the RSSI value is stable
 * before fetching
 */
#define DELAY_BETWEEN_EACH_INST_RSSI_FETCH_US ( 800 )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

static void spectrum_display_start( uint32_t freq_hz );
static void print_configuration( void );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/**
 * @brief Main application entry point.
 */
void lora_spetrum_display_init( void )
{
    lora_init_io_context( &lr1121 ); // Initialize the I/O context for the LR1121
    lora_init_io(&lr1121);           // Initialize the I/O for the LR1121
    lora_spi_init(&lr1121);          // Initialize the SPI interface for the LR1121


    printf( "===== LR11xx Spectrum Display example =====\n\n" );
    printf( "LR11XX driver version: %s\n", lr11xx_driver_version_get_version_string( ) );

    // Initialize the system
    lora_system_init(&lr1121);
    lora_print_version( &lr1121 );
    // Initialize the LoRa radio
    lora_radio_init( &lr1121 );

    print_configuration( );

    create_canvas( );
}

void lora_spetrum_display_loop( void )
{
    int8_t         result;
    static uint8_t freq_chan_index = 0;
    const uint32_t freq_hz         = FREQ_START_HZ + ( freq_chan_index * WIDTH_CHAN_HZ );

    /* Start Spectral scan */
    spectrum_display_start( freq_hz );

    delayMicroseconds( DELAY_BETWEEN_EACH_INST_RSSI_FETCH_US );
    ASSERT_LR11XX_RC( lr11xx_radio_get_rssi_inst( &lr1121, &result ) );
    plot_curve( ( freq_chan_index + 1 ), ( abs( result ) / RSSI_SCALE ) );

    /* Switch to next channel */
    ASSERT_LR11XX_RC( lr11xx_system_set_standby( &lr1121, LR11XX_SYSTEM_STANDBY_CFG_XOSC ) );

    freq_chan_index++;
    if( freq_chan_index >= NB_CHAN )
    {
        freq_chan_index = 0;

        /* Pace the scan speed (1 sec min) */
        for( uint16_t i = 0; i < ( int ) ( PACE_S ? PACE_S : 1 ); i++ )
        {
            delay( 1000 );
        }
    }
}
void spectrum_display_start( uint32_t freq_hz )
{
    /* Set frequency */
    ASSERT_LR11XX_RC( lr11xx_radio_set_rf_freq( &lr1121, freq_hz ) );

    /* Set Radio in Rx continuous mode */
    ASSERT_LR11XX_RC( lr11xx_radio_set_rx_with_timeout_in_rtc_step( &lr1121, RX_CONTINUOUS ) );

    delay( DELAY_BETWEEN_SET_RX_AND_VALID_RSSI_MS );
}

void print_configuration( void )
{
    printf( "\n" );
    printf( "Spectral Scan configuration:\n" );
    printf( "  - Number of channels need to scan: %d\n", NB_CHAN );
    printf( "  - Time delay between 2 scans: %d S\n", PACE_S );
    printf( "  - Start frequency: %.3f MHz\n", ( FREQ_START_HZ / 10E5 ) );
    printf( "  - Frequency step of scan channels: %.3f kHz\n", ( WIDTH_CHAN_HZ / 10E2 ) );
    printf( "Start Spectrum Display:\n" );
}
