#include "lr1121_Firmware_update.h"

bool lr1121_is_chip_in_production_mode( uint8_t type );

bool lr1121_is_fw_compatible_with_chip( lr11xx_fw_update_t update, uint16_t bootloader_version );

lr1121_t lr1121;


void lora_init()
{
    lora_init_io_context(&lr1121); // Initialize the I/O context
    lora_init_io(&lr1121);         // Initialize the I/O

    lora_spi_init(&lr1121); // Initialize the SPI interface
    printf("Update LR1121 to modem firmware 0x%06x\n", LR11XX_FIRMWARE_VERSION);

    lr11xx_hal_wakeup(&lr1121);
}

lr11xx_fw_update_status_t lr1121_update_firmware(void *radio, lr11xx_fw_update_t fw_update_direction,
                                                 uint32_t fw_expected, const uint32_t *buffer, uint32_t length)
{
    lr11xx_bootloader_version_t version_bootloader = {0};

    printf("Reset the chip...\n");

    pinMode(((lr1121_t *)radio)->busy, OUTPUT);
    digitalWrite(((lr1121_t *)radio)->busy, LOW );

    lr11xx_system_reset(radio);

    delay(500);
    pinMode(((lr1121_t *)radio)->busy, INPUT);
    delay(100);

    printf("> Reset done!\n");

    lr11xx_bootloader_get_version(radio, &version_bootloader);
    printf("Chip in bootloader mode:\n");
    printf(" - Chip type               = 0x%02X (0xDF for production)\n", version_bootloader.type);
    printf(" - Chip hardware version   = 0x%02X (0x22 for V2C)\n", version_bootloader.hw);
    printf(" - Chip bootloader version = 0x%04X \n", version_bootloader.fw);

    if (lr1121_is_chip_in_production_mode(version_bootloader.type) == false)
    {
        return LR11XX_FW_UPDATE_WRONG_CHIP_TYPE;
    }

    if (lr1121_is_fw_compatible_with_chip(fw_update_direction, version_bootloader.fw) == false)
    {
        return LR11XX_FW_UPDATE_WRONG_CHIP_TYPE;
    };

    lr11xx_bootloader_pin_t pin = {0x00};
    lr11xx_bootloader_chip_eui_t chip_eui = {0x00};
    lr11xx_bootloader_join_eui_t join_eui = {0x00};

    lr11xx_bootloader_read_pin(radio, pin);
    lr11xx_bootloader_read_chip_eui(radio, chip_eui);
    lr11xx_bootloader_read_join_eui(radio, join_eui);

    printf("PIN is     0x%02X%02X%02X%02X\n", pin[0], pin[1], pin[2], pin[3]);
    printf("ChipEUI is 0x%02X%02X%02X%02X%02X%02X%02X%02X\n", chip_eui[0], chip_eui[1], chip_eui[2], chip_eui[3],
           chip_eui[4], chip_eui[5], chip_eui[6], chip_eui[7]);
    printf("JoinEUI is 0x%02X%02X%02X%02X%02X%02X%02X%02X\n", join_eui[0], join_eui[1], join_eui[2], join_eui[3],
           join_eui[4], join_eui[5], join_eui[6], join_eui[7]);

    printf("Start flash erase...\n");
    lr11xx_bootloader_erase_flash(radio);
    printf("> Flash erase done!\n");

    printf("Start flashing firmware...\n");
    lr11xx_bootloader_write_flash_encrypted_full(radio, 0, buffer, length);
    printf("> Flashing done!\n");

    printf("Rebooting...\n");
    lr11xx_bootloader_reboot(radio, false);
    printf("> Reboot done!\n");

    switch (fw_update_direction)
    {
      case LR1121_FIRMWARE_UPDATE_TO_TRX:
      {
          lr11xx_system_version_t version_trx = {0x00};
          lr11xx_system_uid_t uid = {0x00};

          lr11xx_system_get_version(radio, &version_trx);
          printf("Chip in transceiver mode:\n");
          printf(" - Chip type             = 0x%02X\n", version_trx.type);
          printf(" - Chip hardware version = 0x%02X\n", version_trx.hw);
          printf(" - Chip firmware version = 0x%04X\n", version_trx.fw);

          lr11xx_system_read_uid(radio, uid);

          if (version_trx.fw == fw_expected)
          {
              return LR11XX_FW_UPDATE_OK;
          }
          else
          {
              return LR11XX_FW_UPDATE_ERROR;
          }
          break;
      }
      case LR1121_FIRMWARE_UPDATE_TO_MODEM_V2:
      {
        lr1121_modem_version_t version_modem = { 0 };

        delay( 2000 );
        lr1121_modem_get_modem_version( radio, &version_modem );
        printf( "Chip in LoRa Basics Modem-E mode:\n" );
        printf( " - Chip use case version: 0x%02X\n", version_modem.use_case );
        printf( " - Chip modem major version: 0x%02X\n", version_modem.modem_major );
        printf( " - Chip modem minor version: 0x%02X\n", version_modem.modem_minor );
        printf( " - Chip modem patch version: 0x%02X\n", version_modem.modem_patch );
        printf( " - Chip lbm major version: 0x%02X\n", version_modem.lbm_major );
        printf( " - Chip lbm minor version: 0x%02X\n", version_modem.lbm_minor );
        printf( " - Chip lbm patch version: 0x%02X\n", version_modem.lbm_patch );

        uint32_t fw_version =
            ( ( uint32_t )( version_modem.use_case ) << 24 ) + ( ( uint32_t )( version_modem.modem_major ) << 16 ) +
            ( ( uint32_t )( version_modem.modem_minor << 8 ) ) + ( uint32_t )( version_modem.modem_patch );

        if( fw_version == fw_expected )
        {
            return LR11XX_FW_UPDATE_OK;
        }
        else
        {
            return LR11XX_FW_UPDATE_ERROR;
        }
        break;
      }


    }
    

    return LR11XX_FW_UPDATE_ERROR;
}


bool lr1121_is_chip_in_production_mode( uint8_t type )
{
    return ( type == LR1121_TYPE_PRODUCTION_MODE ) ? true : false;
}

bool lr1121_is_fw_compatible_with_chip( lr11xx_fw_update_t update, uint16_t bootloader_version )
{
    if( ( ( update == LR1110_FIRMWARE_UPDATE_TO_TRX ) || ( update == LR1110_FIRMWARE_UPDATE_TO_MODEM_V1 ) ) &&
        ( bootloader_version != 0x6500 ) )
    {
        return false;
    }
    else if( ( update == LR1120_FIRMWARE_UPDATE_TO_TRX ) && ( bootloader_version != 0x2000 ) )
    {
        return false;
    }
    else if( ( ( update == LR1121_FIRMWARE_UPDATE_TO_TRX ) || ( update == LR1121_FIRMWARE_UPDATE_TO_MODEM_V2 ) ) &&
             ( bootloader_version != 0x2100 ) )
    {
        return false;
    }

    return true;
}