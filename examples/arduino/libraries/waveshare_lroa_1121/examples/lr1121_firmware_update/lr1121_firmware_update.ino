#include "lr1121_Firmware_update.h"

void setup()
{
    Serial.begin(115200); // Initialize serial communication
    lora_init();
    delay(2000);
}

bool is_updated = false;
void loop()
{
    if( is_updated == false )
    {
        const lr11xx_fw_update_status_t status = lr1121_update_firmware(
            &lr1121, LR11XX_FIRMWARE_UPDATE_TO, LR11XX_FIRMWARE_VERSION, lr11xx_firmware_image,
            sizeof( lr11xx_firmware_image ) / sizeof( lr11xx_firmware_image[0] ) );

        switch( status )
        {
        case LR11XX_FW_UPDATE_OK:
            printf( "Expected firmware running!\n" );
            printf( "Please flash another application (like EVK Demo App).\n" );
            break;
        case LR11XX_FW_UPDATE_WRONG_CHIP_TYPE:
            printf( "Wrong chip type!\n" );
            break;
        case LR11XX_FW_UPDATE_ERROR:
            printf( "Error! Wrong firmware version - please retry.\n" );
            break;
        }
        is_updated = true;
    }
}
