/*!
 * @file      lr1121_ping_pong.h
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

#ifndef MAIN_PER_H
#define MAIN_PER_H


/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */
#include "lr1121_config.h"

/**
 * @brief External flag to indicate if an interrupt has occurred
 * This flag is set by the interrupt service routine (ISR) and checked in the main loop.
 */
extern bool irq_flag;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

#ifndef RX_TIMEOUT_VALUE
#define RX_TIMEOUT_VALUE 600
#endif

/**
 * @brief Size of ping-pong message prefix
 *
 * Expressed in bytes
 */
#define PING_PONG_PREFIX_SIZE 5

/**
 * @brief Threshold:number of exchanges before informing user on UART that the board pair is still not synchronized
 *
 * Expressed in number of packet exchanged
 */
#define SYNC_PACKET_THRESHOLD 64

/**
 * @brief Number of exchanges are stored in the payload of the packet exchanged during this PING-PONG communication
 *        this constant indicates where in the packet the two bytes used to count are located
 *
 * Expressed in bytes
 */

#define ITERATION_INDEX ( PING_PONG_PREFIX_SIZE + 1 )

/**
 * @brief Duration of the wait before packet transmission to assure reception status is ready on the other side
 *
 * Expressed in milliseconds
 */
#define DELAY_BEFORE_TX_MS 20

/**
 * @brief Duration of the wait between each ping-pong activity, can be used to adjust ping-pong speed
 *
 * Expressed in milliseconds
 */
#define DELAY_PING_PONG_PACE_MS 200

/**
 * @brief LR11xx interrupt mask used by the application
 */
#define IRQ_MASK                                                                          \
    ( LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_RX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT | \
      LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_CRC_ERROR | LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR )

#define HAL_DBG_TRACE_ARRAY( msg, array, len )                                \
    do                                                                        \
    {                                                                         \
        printf( "%s - (%lu bytes):\n", msg, ( uint32_t ) len ); \
        for( uint32_t i = 0; i < ( uint32_t ) len; i++ )                      \
        {                                                                     \
            if( ( ( i % 16 ) == 0 ) && ( i > 0 ) )                            \
            {                                                                 \
                printf( "\n" );                                 \
            }                                                                 \
            printf( " %02X", array[i] );                        \
        }                                                                     \
        printf( "\n" );                                         \
    } while( 0 );
    
/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define APP_PARTIAL_SLEEP true

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */
void lr1121_ping_pong_init( void );
void lora_irq_process( const void* context, lr11xx_system_irq_mask_t irq_filter_mask );
#endif  // MAIN_PER_H

/* --- EOF ------------------------------------------------------------------ */
