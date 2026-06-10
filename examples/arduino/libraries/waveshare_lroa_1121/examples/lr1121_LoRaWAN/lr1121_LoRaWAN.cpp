#include "lr1121_LoRaWAN.h"

#if( !USE_LR11XX_CREDENTIALS )
/**
 * @brief Stack credentials
 */
static const uint8_t user_join_eui[8] = LORAWAN_JOIN_EUI;
static const uint8_t user_dev_eui[8]  = LORAWAN_DEVICE_EUI;
static const uint8_t user_nwk_key[16] = LORAWAN_NWK_KEY;
static const uint8_t user_app_key[16] = LORAWAN_APP_KEY;
#endif


#if( USE_LR11XX_CREDENTIALS )
/**
 * @brief Internal credentials
 */
static uint8_t chip_eui[8] = { 0 };
static uint8_t chip_pin[4] = { 0 };
#endif


lr1121_t lr1121;

static volatile bool user_button_is_press = false;  // Flag indicating if the button is pressed
static uint32_t      uplink_counter       = 0;      // Counter for uplinks sent
static uint32_t      confirmed_counter    = 0;      // Counter for confirmed uplinks
static bool          uplink_sending       = false;  // Flag indicating an uplink is requested but not yet sent

/**
 * @brief User callback for button EXTI
 *
 * @param context Define by the user at the init
 */
static void ARDUINO_ISR_ATTR user_button_callback( )
{
    user_button_is_press    = true;
}


/**
 * @brief Send the 32bits uplink counter and 32bits confirmed counter on chosen port
 */
static void send_uplinks_counter_on_port( uint8_t port );

/**
 * @brief Send tx_frame_buffer on choosen port
 *
 */
static lr1121_modem_response_code_t send_frame( const uint8_t* tx_frame_buffer, const uint8_t tx_frame_buffer_size,
                                                uint8_t port, const lr1121_modem_uplink_type_t tx_confirmed );

/**
 * @brief Process received events
 *
 */
static void event_process( void* context );

static bool irq_flag = false;

void ARDUINO_ISR_ATTR isr() {
    irq_flag = true; // Set the interrupt flag
    
}

void lr1121_LoRaWAN_init()
{
    lora_init_io_context( &lr1121 );
    lora_init_io(&lr1121);
    
    lora_spi_init(&lr1121);

    printf("###### ===== Periodical uplink (%d sec) example is starting ==== ######\n\n\n",PERIODICAL_UPLINK_DELAY_S);

    //Set external buttons
    pinMode(EXTI_BUTTON, INPUT_PULLUP);
    attachInterrupt(EXTI_BUTTON, user_button_callback, RISING);

    lora_init_irq(&lr1121, isr);     // Initialize the interrupt service routine
    
    // Flush events before enabling irq
    lr1121_modem_board_event_flush( &lr1121 );

    printf( "Initialization done\n\n" );
    lr1121_modem_system_reboot( &lr1121, false );
}

void lr1121_LoRaWAN_loop()
{
  if( user_button_is_press == true )
    {
        user_button_is_press = false;
        lr1121_modem_lorawan_status_t modem_status;
        lr1121_modem_get_status( &lr1121, (lr1121_modem_lorawan_status_bitmask_t*)&modem_status );
        // Check if the device has already joined a network
        if( ( modem_status & LR1121_LORAWAN_JOINED ) == LR1121_LORAWAN_JOINED )
        {
            if( uplink_sending == false )
            {
                // Send the uplink counter on port 102
                printf("102\r\n");
                send_uplinks_counter_on_port( 102 );
            }
            else
            {
                printf(
                    "An uplink has already been requested, please wait for this uplink to be sent before "
                    "requesting another one\n" );
            }
        }
        else
        {
            printf( "The device has not yet joined the network.\n" );
        }
    }

    if(irq_flag)
    {
        event_process(&lr1121);
        irq_flag = false;
    }
//   delay(100);
}

static void event_process( void* context )
{
    // Continue to read modem events until all of them have been processed.
    lr1121_modem_response_code_t rc_event = LR1121_MODEM_RESPONSE_CODE_OK;
    do
    {
        // Read modem event
        lr1121_modem_event_fields_t current_event;

        rc_event = lr1121_modem_get_event( context, &current_event );
        if( rc_event == LR1121_MODEM_RESPONSE_CODE_OK )
        {
            uint8_t tmp_pin[4] = { 0 };  // The chip_pin is not used if we use custom credentials
            uint8_t tmp_join_eui[8] = { 0 };

            uint8_t rx_payload[LORAWAN_APP_DATA_MAX_SIZE] = { 0 };  // Buffer for rx payload
            uint8_t rx_payload_size                       = 0;      // Size of the payload in the rx_payload buffer
            lr1121_modem_downlink_metadata_t rx_metadata  = { 0 };  // Metadata of downlink
            uint8_t                          rx_remaining = 0;      // Remaining downlink payload in modem

            uint8_t adr_custom_list[16] = { 0 };
            switch( current_event.event_type )
            {
            case LR1121_MODEM_LORAWAN_EVENT_RESET:

                printf( "Event received: RESET\n\n");
                
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_rf_output(context,LR1121_MODEM_BSP_RADIO_PA_SEL_HP) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_system_cfg_lfclk( context, LR1121_MODEM_SYSTEM_LFCLK_EXT, true ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_crystal_error( context, 50 ) );
                get_and_print_crashlog( context );
                

#if( !USE_LR11XX_CREDENTIALS )
                // Set user credentials
                printf( "###### ===== LR1121 SET EUI and KEYS ==== ######\n\n" );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_dev_eui( context, user_dev_eui ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_join_eui( context, user_join_eui ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_app_key( context, user_app_key ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_nwk_key( context, user_nwk_key ) );
               
                print_lorawan_credentials( user_dev_eui, user_join_eui, tmp_pin, USE_LR11XX_CREDENTIALS );
#else
                // Get internal credentials

                ASSERT_SMTC_MODEM_RC( lr1121_modem_system_read_uid( context, chip_eui ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_system_read_pin( context, chip_pin ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_get_join_eui( context, tmp_join_eui ) );
                print_lorawan_credentials( chip_eui, tmp_join_eui, chip_pin, USE_LR11XX_CREDENTIALS );
#endif

                // Set user region
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_region( context, LORAWAN_REGION_USED ) );
                print_lorawan_region( LORAWAN_REGION_USED );
                // Schedule a LoRaWAN network JoinRequest.
                ASSERT_SMTC_MODEM_RC( lr1121_modem_join( context ) );
                printf( "###### ===== JOINING ==== ######\n\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_ALARM:
                printf( "Event received: ALARM\n\n" );
                // Send periodical uplink on port 101
                send_uplinks_counter_on_port( 101 );
                // Restart periodical uplink alarm
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_alarm_timer( context, PERIODICAL_UPLINK_DELAY_S ) );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_JOINED:
                printf( "Event received: JOINED\n" );
                printf( "Modem is now joined \n\n" );

                
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_adr_profile(
                    context, LR1121_MODEM_ADR_PROFILE_NETWORK_SERVER_CONTROLLED, adr_custom_list ) );

                // Send first periodical uplink on port 101
                send_uplinks_counter_on_port( 101 );
                // start periodical uplink alarm
                ASSERT_SMTC_MODEM_RC( lr1121_modem_set_alarm_timer( context, PERIODICAL_UPLINK_DELAY_S ) );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_TX_DONE:
            {
                const lr1121_modem_tx_done_event_t tx_done_event_data =
                    ( lr1121_modem_tx_done_event_t )( current_event.data >> 8 );
                printf( "Event received: TXDONE\n\n" );

                printf( "TX DATA     : " );

                switch( tx_done_event_data )
                {
                case LR1121_MODEM_TX_NOT_SENT:
                {
                    printf( " NOT SENT" );
                    uplink_counter--;
                    break;
                }
                case LR1121_MODEM_CONFIRMED_TX:
                {
                    printf( " CONFIRMED - ACK" );
                    confirmed_counter++;
                    break;
                }
                case LR1121_MODEM_UNCONFIRMED_TX:
                {
                    printf( " UNCONFIRMED\n\n" );
                    break;
                }
                default:
                {
                    printf( " unknown value (%02x)\n\n", tx_done_event_data );
                }
                }
                printf( "\n\n" );

                printf( "Transmission done \n" );
                uplink_sending = false;  // Reset flag indicating an uplink request has been processed
                break;
            }

            case LR1121_MODEM_LORAWAN_EVENT_DOWN_DATA:
                printf( "Event received: DOWNDATA\n\n" );
                // Get downlink data
                ASSERT_SMTC_MODEM_RC( lr1121_modem_get_downlink_data_size( context, &rx_payload_size, &rx_remaining ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_get_downlink_data( context, rx_payload, rx_payload_size ) );
                ASSERT_SMTC_MODEM_RC( lr1121_modem_get_downlink_metadata( context, &rx_metadata ) );
                printf( "Data received on port %u\n", rx_metadata.fport );
                printf( "Received payload- (%lu bytes): \r\n", rx_payload_size );
                for( uint8_t i = 0; i < rx_payload_size; i++ )                      
                {                                                                     
                    if( ( ( i % 16 ) == 0 ) && ( i > 0 ) )                            
                    {                                                                 
                        printf( "\n" );                                 
                    }                                                                
                    printf( " %02X", rx_payload[i] );                        
                }                                                                     
                printf( "\n" );  
                break;

            case LR1121_MODEM_LORAWAN_EVENT_JOIN_FAIL:
                printf( "Event received: JOINFAIL\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_LINK_CHECK:
                printf( "Event received: LINK_CHECK\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_CLASS_B_PING_SLOT_INFO:
                printf( "Event received: CLASS_B_PING_SLOT_INFO\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_CLASS_B_STATUS:
                printf( "Event received: CLASS_B_STATUS\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_LORAWAN_MAC_TIME:
                printf( "Event received: LORAWAN MAC TIME\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_NEW_MULTICAST_SESSION_CLASS_C:
                printf( "Event received: MULTICAST CLASS_C STOP\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_NEW_MULTICAST_SESSION_CLASS_B:
                printf( "Event received: MULTICAST CLASS_B STOP\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_C:
                printf( "Event received: New MULTICAST CLASS_C\n\n" );
                break;

            case LR1121_MODEM_LORAWAN_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_B:
                printf( "Event received: New MULTICAST CLASS_B\n\n" );
                break;
            default:
                printf( "Event not handled 0x%02x\n", current_event.event_type );
                break;
            }
        }
    } while( rc_event != LR1121_MODEM_RESPONSE_CODE_NO_EVENT );
}

static void send_uplinks_counter_on_port( uint8_t port )
{
    // Send uplink and confirmed counter
    uint8_t buff[8] = { 0 };
    buff[0]         = ( uplink_counter >> 24 ) & 0xFF;
    buff[1]         = ( uplink_counter >> 16 ) & 0xFF;
    buff[2]         = ( uplink_counter >> 8 ) & 0xFF;
    buff[3]         = ( uplink_counter & 0xFF );
    buff[4]         = ( confirmed_counter >> 24 ) & 0xFF;
    buff[5]         = ( confirmed_counter >> 16 ) & 0xFF;
    buff[6]         = ( confirmed_counter >> 8 ) & 0xFF;
    buff[7]         = ( confirmed_counter & 0xFF );
    ASSERT_SMTC_MODEM_RC( send_frame( buff, 8, port, (lr1121_modem_uplink_type_t)true ) );
    uplink_counter++;  // Increment uplink counter
    uplink_sending = true;
}

static lr1121_modem_response_code_t send_frame( const uint8_t* tx_frame_buffer, const uint8_t tx_frame_buffer_size,
                                                uint8_t port, const lr1121_modem_uplink_type_t tx_confirmed )
{
    lr1121_modem_response_code_t modem_response_code = LR1121_MODEM_RESPONSE_CODE_OK;
    uint8_t                      tx_max_payload;
    int32_t                      duty_cycle;

    lr1121_modem_get_duty_cycle_status( &lr1121, &duty_cycle );

    if( duty_cycle < 0 )
    {
        printf( "DUTY CYCLE, NEXT UPLINK AVAILABLE in %d milliseconds \n\n\n", -duty_cycle );
        return modem_response_code;
    }

    modem_response_code = lr1121_modem_get_next_tx_max_payload( &lr1121, &tx_max_payload );
    if( modem_response_code != LR1121_MODEM_RESPONSE_CODE_OK )
    {
        printf( "\n\n lr1121_modem_get_next_tx_max_payload RC : %d \n\n", modem_response_code );
    }

    if( tx_frame_buffer_size > tx_max_payload )
    {
        /* Send empty frame in order to flush MAC commands */
        printf( "\n\n APP DATA > MAX PAYLOAD AVAILABLE (%d bytes) \n\n", tx_max_payload );
        modem_response_code = lr1121_modem_request_tx( &lr1121, port, tx_confirmed, NULL, 0 );
    }
    else
    {
        modem_response_code =
            lr1121_modem_request_tx( &lr1121, port, tx_confirmed, tx_frame_buffer, tx_frame_buffer_size );
    }

    if( modem_response_code == LR1121_MODEM_RESPONSE_CODE_OK )
    {
        printf( "lr1121 MODEM-E REQUEST TX \n\n" );
        printf( "TX DATA     : " );
        print_hex_buffer( tx_frame_buffer, tx_frame_buffer_size );
        printf( "\n\n\n" );
    }
    else
    {
        printf( "lr1121 MODEM-E REQUEST TX ERROR CMD, modem_response_code : %d \n\n\n",
                             modem_response_code );
    }
    return modem_response_code;
}
