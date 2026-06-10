#ifndef _RADIOLIB_EX_LORAWAN_CONFIG_H
#define _RADIOLIB_EX_LORAWAN_CONFIG_H

#include <RadioLib.h>
#include "esp_log.h"

// LoRaWAN parameters
const uint32_t uplinkIntervalSeconds = 5UL * 60UL;

// joinEUI - previous versions of LoRaWAN called this AppEUI
// for development purposes you can use all zeros - see wiki for details
#define RADIOLIB_LORAWAN_JOIN_EUI  0xb3570e6c90228b5d

// the Device EUI & two keys can be generated on the TTN console 
#ifndef RADIOLIB_LORAWAN_DEV_EUI   // Replace with your Device EUI
#define RADIOLIB_LORAWAN_DEV_EUI   0xffb4509226b30748
#endif
#ifndef RADIOLIB_LORAWAN_APP_KEY   // Replace with your App Key 
#define RADIOLIB_LORAWAN_APP_KEY   0x91, 0x35, 0x84, 0xa1, 0x10, 0x4c, 0x57, 0x98, 0xaf, 0xbf, 0xf9, 0x5b, 0x31, 0x9f, 0xd4, 0xb9
#endif
#ifndef RADIOLIB_LORAWAN_NWK_KEY   // Put your Nwk Key here
#define RADIOLIB_LORAWAN_NWK_KEY   0x91, 0x35, 0x84, 0xa1, 0x10, 0x4c, 0x57, 0x98, 0xaf, 0xbf, 0xf9, 0x5b, 0x31, 0x9f, 0xd4, 0xb9
#endif
const LoRaWANBand_t Region = EU868;
const uint8_t subBand = 0;

uint64_t joinEUI =   RADIOLIB_LORAWAN_JOIN_EUI;
uint64_t devEUI  =   RADIOLIB_LORAWAN_DEV_EUI;
uint8_t appKey[] = { RADIOLIB_LORAWAN_APP_KEY };
uint8_t nwkKey[] = { RADIOLIB_LORAWAN_NWK_KEY };

class EspHal;
extern EspHal* hal;
extern LR1121 radio;
extern LoRaWANNode node;

#endif
