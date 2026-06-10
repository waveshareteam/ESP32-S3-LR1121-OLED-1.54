#ifndef WAVESHARE_LORA_SPI_H
#define WAVESHARE_LORA_SPI_H

#include <Arduino.h>
#include <SPI.h>

#include "lr11xx_driver/lr11xx_bootloader.h"
#include "lr11xx_driver/lr11xx_hal.h"
#include "lr11xx_driver/lr11xx_system.h"
#include "lr11xx_driver/lr11xx_radio.h"
#include "lr11xx_driver/lr11xx_regmem.h"
#include "lr11xx_driver/lr11xx_lr_fhss.h"
#include "lr11xx_driver/lr11xx_driver_version.h"


#include "lr1121_modem/lr1121_modem_helper.h"
#include "lr1121_modem/lr1121_modem_system_types.h"

#include "lr1121_modem/lr1121_modem_common.h"
#include "lr1121_modem/lr1121_modem_modem.h"
#include "lr1121_modem/lr1121_modem_hal.h"
#include "lr1121_modem/lr1121_modem_system.h"
#include "lr1121_modem/lr1121_modem_bsp.h"
#include "lr1121_modem/lr1121_modem_radio.h"

#include "lr1121_printers/lr11xx_bootloader_types_str.h"
#include "lr1121_printers/lr11xx_crypto_engine_types_str.h"
#include "lr1121_printers/lr11xx_lr_fhss_types_str.h"
#include "lr1121_printers/lr11xx_radio_types_str.h"
#include "lr1121_printers/lr11xx_rttof_types_str.h"
#include "lr1121_printers/lr11xx_system_types_str.h"
#include "lr1121_printers/lr11xx_types_str.h"
#include "lr1121_printers/lr11xx_printf_info.h"
#include "lr1121_printers/lr1121_modem_printf_info.h"

#include "lr1121_common/lr1121_common.h"

// #define USE_LR11XX_CRC_OVER_SPI

// #define RADIO_RESET 16 
// #define RADIO_MOSI 6
// #define RADIO_MISO 5 
// #define RADIO_CLK 7 
// #define RADIO_CS 15 
// #define RADIO_BUSY 39 
// #define RADIO_IRQ 40
// #define RADIO_LED 41

#define RADIO_RESET 39 
#define RADIO_MOSI 45
#define RADIO_MISO 46 
#define RADIO_CLK 40 
#define RADIO_CS 42 
#define RADIO_BUSY 41 
#define RADIO_IRQ 38
#define RADIO_LED -1

#define RADIO_SPI_SPEED 8 * 1000 * 1000

typedef struct lr1121_s
{
    uint8_t     reset;
    uint8_t     busy;
    uint8_t     irq;
    uint8_t     mosi;
    uint8_t     miso;
    uint8_t     clk;
    uint8_t     cs;
    uint8_t     led;
    // spi_device_handle_t spi;
} lr1121_t;

/**
 * @brief Initializes the radio I/Os pins context
 *
 * @param [in] context Radio abstraction
 */
void lora_init_io_context(const void* context );

/**
 * @brief Initializes the radio I/Os pins interface
 *
 * @param [in] context Radio abstraction
 */
void lora_init_io( const void* context );

void lora_init_irq(const void *context, voidFuncPtr handler);

void lora_spi_init(const void* context);
void lora_spi_write_bytes(const void* context,const uint8_t *wirte,const uint16_t wirte_length);
void lora_spi_read_bytes(const void* context, uint8_t *read,const uint16_t read_length);
/**
 * @brief Flush the modem event queue
 *
 * @param [in] context Radio abstraction
 *
 * @returns Modem-E response code
 */
lr1121_modem_response_code_t lr1121_modem_board_event_flush( const void* context );

#endif