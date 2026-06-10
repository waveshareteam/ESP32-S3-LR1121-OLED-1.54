#ifndef LR1121_FIRMWARE_UPDATE_H
#define LR1121_FIRMWARE_UPDATE_H

#include "wavesahre_lora_1121.h"

#include "lr1121_transceiver_0101.h"
// #include "lr1121_transceiver_0102.h"
// #include "lr1121_transceiver_0103.h"
// #include "lr1121_modem_05020001.h"

#define LR1121_TYPE_PRODUCTION_MODE 0xDF

extern lr1121_t lr1121;

typedef enum
{
    LR1110_FIRMWARE_UPDATE_TO_TRX,
    LR1110_FIRMWARE_UPDATE_TO_MODEM_V1,
    LR1120_FIRMWARE_UPDATE_TO_TRX,
    LR1121_FIRMWARE_UPDATE_TO_TRX,
    LR1121_FIRMWARE_UPDATE_TO_MODEM_V2,
} lr11xx_fw_update_t;

typedef enum
{
    LR11XX_FW_UPDATE_OK              = 0,
    LR11XX_FW_UPDATE_WRONG_CHIP_TYPE = 1,
    LR11XX_FW_UPDATE_ERROR           = 2,
} lr11xx_fw_update_status_t;

void lora_init();

lr11xx_fw_update_status_t lr1121_update_firmware( void* radio, lr11xx_fw_update_t fw_update_direction,
    uint32_t fw_expected, const uint32_t* buffer, uint32_t length );

    
#endif  // LR1121_FIRMWARE_UPDATE_H
