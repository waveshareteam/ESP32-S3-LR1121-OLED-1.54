/*!
 * @file      lr1121_per.h
 *
 * @brief     Packet Error Rate (PER) example for LR11xx chip
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
#include "lr1121_config.h" // Include the LR1121 configuration file

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

#ifndef RECEIVER
#define RECEIVER 1 // 0：send  1:read
#endif

#define TRANSMITTER !RECEIVER // Define TRANSMITTER as the opposite of RECEIVER

#if( TRANSMITTER == RECEIVER )
#error "Please define only Transmitter or Receiver." // Ensure only one mode is defined
#endif

/**
 * @brief LR11xx interrupt mask used by the application
 * This mask defines which interrupts the application is interested in handling.
 */
#define IRQ_MASK                                                                                               \
    ( LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_RX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT |                      \
      LR11XX_SYSTEM_IRQ_PREAMBLE_DETECTED | LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR | \
      LR11XX_SYSTEM_IRQ_CRC_ERROR )

#ifndef RX_TIMEOUT_VALUE
#define RX_TIMEOUT_VALUE 1000 // Define the default RX timeout value in milliseconds
#endif

/*! 
 * @brief Delay in ms between the end of a transmission and the beginning of the next one
 */
#ifndef TX_TO_TX_DELAY_IN_MS
#define TX_TO_TX_DELAY_IN_MS 200 // Define the default delay between transmissions in milliseconds
#endif

/*! 
 * @brief Number of frames expected on the receiver side
 */
#ifndef NB_FRAME
#define NB_FRAME 20 // Define the default number of frames for the PER test
#endif

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
void per_test(); // Prototype for the PER test function
void lora_irq_process(const void* context, lr11xx_system_irq_mask_t irq_filter_mask); // Prototype for the IRQ processing function

#endif