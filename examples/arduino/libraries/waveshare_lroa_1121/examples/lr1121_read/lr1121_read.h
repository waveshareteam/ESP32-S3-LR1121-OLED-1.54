/*!
 * @file      lr1121_read.h
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

#ifndef LR1121_READ_H
#define LR1121_READ_H

#include "lr1121_config.h"

/**
 * @brief LR11xx interrupt mask used by the application
 * This mask defines which interrupts the application is interested in handling.
 */
#define IRQ_MASK                                                                          \
    (   LR11XX_SYSTEM_IRQ_RX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT | \
        LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_CRC_ERROR )

/**
 * @brief External flag to indicate if an interrupt has occurred
 * This flag is set by the interrupt service routine (ISR) and checked in the main loop.
 */
extern bool irq_flag;

/**
 * @brief Initialize the LR1121 for reading
 * This function initializes the LR1121 module, sets up the necessary configurations,
 * and prepares the module for receiving data.
 */
void lora_read_init();

/**
 * @brief Process the LR1121 interrupt
 *
 * This function processes the interrupt status of the LR1121 module. It checks for various interrupt types,
 * including RX timeout, header error, CRC error, RX done, and TX done. It calls the appropriate handler functions
 * based on the interrupt type.
 *
 * @param context Pointer to the LR1121 context structure
 * @param irq_filter_mask Mask to filter the IRQ status
 */
void lora_irq_process( const void* context, lr11xx_system_irq_mask_t irq_filter_mask);

#endif