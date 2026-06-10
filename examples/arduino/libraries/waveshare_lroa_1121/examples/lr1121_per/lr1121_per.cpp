/*!
 * @file      lr1121_per.cpp
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

#include "lr1121_per.h"

static uint8_t buffer[PAYLOAD_LENGTH]; // Buffer to store the payload

// Counters for various reception outcomes
static uint16_t nb_ok            = 0; // Number of successful receptions
static uint16_t nb_rx_timeout    = 0; // Number of RX timeouts
static uint16_t nb_rx_error      = 0; // Number of RX errors (e.g., CRC errors)
static uint16_t nb_fsk_len_error = 0; // Number of FSK length errors

static uint8_t rolling_counter = 0; // Rolling counter for packet sequence

static uint16_t per_index      = 0; // Index for PER calculation
static bool     first_pkt_flag = false; // Flag to indicate the first packet reception

static uint8_t  per_msg[PAYLOAD_LENGTH]; // Message for PER test
static uint32_t rx_timeout = RX_TIMEOUT_VALUE; // RX timeout value

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/**
 * @brief Handle reception failure for PER example
 *
 * @param [in] failure_counter pointer to the counter for each type of reception failure
 */
static void per_reception_failure_handling(uint16_t* failure_counter);

/**
 * @brief Interface to read bytes from the RX buffer
 *
 * @param [in] buffer Pointer to a byte array to be filled with content from memory
 * @param [in] buffer_length Length of the buffer to contain payload data
 * @param [in] size Number of bytes to read from memory
 */
void apps_common_lr11xx_receive(uint8_t* buffer, uint8_t buffer_length, uint8_t* size)
{
    lr11xx_radio_rx_buffer_status_t rx_buffer_status;
    lr11xx_radio_pkt_status_lora_t  pkt_status_lora;
    lr11xx_radio_pkt_status_gfsk_t  pkt_status_gfsk;

    // Get the RX buffer status
    lr11xx_radio_get_rx_buffer_status(&lr1121, &rx_buffer_status);
    *size = rx_buffer_status.pld_len_in_bytes;

    // Check if the received payload size exceeds the buffer size
    if (*size > buffer_length)
    {
        printf("Received payload (size: %d) is bigger than the buffer (size: %d)!\n", *size, buffer_length);
        return;
    }

    // Read the received data from the FIFO into the buffer
    lr11xx_regmem_read_buffer8(&lr1121, buffer, rx_buffer_status.buffer_start_pointer, rx_buffer_status.pld_len_in_bytes);

    printf("Packet content: ");
    for (int i = 0; i < *size; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    printf("Packet status:\n");
    if (PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_LORA)
    {
        // Get LoRa packet status
        lr11xx_radio_get_lora_pkt_status(&lr1121, &pkt_status_lora);
        printf("  - RSSI packet = %i dBm\n", pkt_status_lora.rssi_pkt_in_dbm);
        printf("  - Signal RSSI packet = %i dBm\n", pkt_status_lora.signal_rssi_pkt_in_dbm);
        printf("  - SNR packet = %i dB\n", pkt_status_lora.snr_pkt_in_db);
    }
    else if (PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_GFSK)
    {
        // Get GFSK packet status
        lr11xx_radio_get_gfsk_pkt_status(&lr1121, &pkt_status_gfsk);
        printf("  - RSSI average = %i dBm\n", pkt_status_gfsk.rssi_avg_in_dbm);
        printf("  - RSSI sync = %i dBm\n", pkt_status_gfsk.rssi_sync_in_dbm);
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void per_test()
{
    lora_init_io_context(&lr1121); // Initialize the I/O context
    lora_init_io(&lr1121);         // Initialize the I/O

    lora_spi_init(&lr1121);        // Initialize the SPI interface
    // Initialize the system
    lora_system_init(&lr1121);
    // Print the version number for verification
    lora_print_version(&lr1121);

    // Initialize the LoRa radio
    lora_radio_init(&lr1121);

    // Configure the DIO9 interrupt for RX done, timeout, and errors
    lr11xx_system_set_dio_irq_params(&lr1121, IRQ_MASK, 0);
    // Clear all pending IRQ statuses
    lr11xx_system_clear_irq_status(&lr1121, LR11XX_SYSTEM_IRQ_ALL_MASK);

    // Prepare the payload buffer
    for (int i = 1; i < PAYLOAD_LENGTH; i++)
    {
        buffer[i] = i;
    }

    // Adjust the reception timeout to account for the time on air
    rx_timeout += get_time_on_air_in_ms();

#if RECEIVER == 1
    // Receiver mode
    printf("RECEIVER\r\n");
    lr11xx_radio_set_rx(&lr1121, rx_timeout); // Start receiving
    memcpy(per_msg, &buffer[1], PAYLOAD_LENGTH - 1); // Copy the message for comparison
#else
    // Transmitter mode
    printf("Transmitter\r\n");
    buffer[0] = 0; // Initialize the rolling counter
    lr11xx_regmem_write_buffer8(&lr1121, buffer, PAYLOAD_LENGTH); // Write the buffer to the FIFO
    lr11xx_radio_set_tx(&lr1121, 0); // Start transmitting
#endif

    // Main loop to process IRQs
    while (per_index < NB_FRAME)
    {
        lora_irq_process(&lr1121, IRQ_MASK);
    }

    // Finalize the PER test
    if (per_index > NB_FRAME) // Adjust the last packet if necessary
    {
        nb_ok--;
    }

    // Display the final PER results
    printf("PER = %d \n", 100 - ((nb_ok * 100) / NB_FRAME));
    printf("Final PER index: %d \n", per_index);
    printf("Valid reception amount: %d \n", nb_ok);
    printf("Timeout reception amount: %d \n", nb_rx_timeout);
    printf("CRC Error reception amount: %d \n", nb_rx_error);
    if (PACKET_TYPE == LR11XX_RADIO_PKT_TYPE_GFSK)
    {
        printf("FSK Length Error reception amount: %d \n", nb_fsk_len_error);
    }
}

void on_tx_done(void)
{
    delay(TX_TO_TX_DELAY_IN_MS); // Delay between transmissions
    buffer[0]++; // Increment the rolling counter
    printf("Counter value: %d\n", buffer[0]);
    ASSERT_LR11XX_RC(lr11xx_regmem_write_buffer8(&lr1121, buffer, PAYLOAD_LENGTH)); // Write the buffer to the FIFO
    ASSERT_LR11XX_RC(lr11xx_radio_set_tx(&lr1121, 0)); // Start transmitting
}

void on_rx_done(void)
{
    uint8_t size;

    // Receive the packet
    apps_common_lr11xx_receive(buffer, PAYLOAD_LENGTH, &size);

    // Check if the received packet matches the expected message
    if (memcmp(&buffer[1], per_msg, PAYLOAD_LENGTH - 1) == 0)
    {
        // Start counting after the first received packet
        if (first_pkt_flag == true)
        {
            uint8_t rolling_counter_gap = (uint8_t)(buffer[0] - rolling_counter);
            nb_ok++;
            per_index += rolling_counter_gap;
            if (rolling_counter_gap > 1)
            {
                printf("%d packet(s) missed\n", (rolling_counter_gap - 1));
            }
            rolling_counter = buffer[0];
        }
        else
        {
            first_pkt_flag = true;
            rolling_counter = buffer[0];
        }
        printf("Counter value: %d, PER index: %d\n", buffer[0], per_index);
    }

    // Re-start RX only if the expected number of frames is not reached
    if (per_index < NB_FRAME)
    {
        ASSERT_LR11XX_RC(lr11xx_radio_set_rx(&lr1121, rx_timeout));
    }
}

void on_rx_timeout(void)
{
    per_reception_failure_handling(&nb_rx_timeout); // Handle RX timeout
}

void on_rx_crc_error(void)
{
    per_reception_failure_handling(&nb_rx_error); // Handle RX CRC error
}

void on_fsk_len_error(void)
{
    per_reception_failure_handling(&nb_fsk_len_error); // Handle FSK length error
}

static void per_reception_failure_handling(uint16_t* failure_counter)
{
    // Start counting failures only after the first packet has been successfully received
    if (first_pkt_flag == true)
    {
        (*failure_counter)++; // Increment the failure counter
    }
    // Reconfigure the radio to start receiving again
    ASSERT_LR11XX_RC(lr11xx_radio_set_rx(&lr1121, rx_timeout));
}


void lora_irq_process(const void* context, lr11xx_system_irq_mask_t irq_filter_mask)
{
    lr11xx_system_irq_mask_t radio_irq;

    // Get and clear the IRQ status
    lr11xx_system_get_and_clear_irq_status(context, &radio_irq);

    // Apply the IRQ filter mask to get the relevant IRQ status
    radio_irq &= irq_filter_mask;

    // Check for RX timeout
    if ((radio_irq & LR11XX_SYSTEM_IRQ_TIMEOUT) == LR11XX_SYSTEM_IRQ_TIMEOUT)
    {
        printf("Rx timeout\r\n");
        on_rx_timeout(); // Call the RX timeout handler
    }

    // Check for header error
    if (radio_irq & LR11XX_SYSTEM_IRQ_HEADER_ERROR)
    {
        printf("header_error\r\n");
    }

    // Check for CRC error
    if (radio_irq & LR11XX_SYSTEM_IRQ_CRC_ERROR)
    {
        printf("crc_error\r\n");
        on_rx_crc_error(); // Call the CRC error handler
    }

    // Check if reception is complete
    if ((radio_irq & LR11XX_SYSTEM_IRQ_RX_DONE) == LR11XX_SYSTEM_IRQ_RX_DONE)
    {
        // Check for CRC error during reception
        if ((radio_irq & LR11XX_SYSTEM_IRQ_CRC_ERROR) == LR11XX_SYSTEM_IRQ_CRC_ERROR)
        {
            printf("CRC error\n");
            on_rx_crc_error(); // Call the CRC error handler
        }
        // Check for FSK length error
        else if ((radio_irq & LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR) == LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR)
        {
            printf("FSK length error\n");
            on_fsk_len_error(); // Call the FSK length error handler
        }
        else
        {
            printf("Rx done\n");
            on_rx_done(); // Call the RX done handler
        }
    }

    // Check if transmission is complete
    if ((radio_irq & LR11XX_SYSTEM_IRQ_TX_DONE) == LR11XX_SYSTEM_IRQ_TX_DONE)
    {
        printf("TX DONE\r\n");
        on_tx_done(); // Call the TX done handler
    }
}