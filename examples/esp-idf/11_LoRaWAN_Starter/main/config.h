/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _RADIOLIB_EX_LORAWAN_CONFIG_H
#define _RADIOLIB_EX_LORAWAN_CONFIG_H

#include <RadioLib.h>
#include "esp_log.h"

/*
 * Put production LoRaWAN credentials in lorawan_secrets.h and keep that file
 * out of version control. The placeholders below allow the example to build.
 */
#if __has_include("lorawan_secrets.h")
#include "lorawan_secrets.h"
#endif

static const uint32_t uplink_interval_seconds = 5UL * 60UL;

// joinEUI - previous versions of LoRaWAN called this AppEUI
// for development purposes you can use all zeros - see wiki for details
#ifndef RADIOLIB_LORAWAN_JOIN_EUI
#define RADIOLIB_LORAWAN_JOIN_EUI  0x0000000000000000
#endif

// the Device EUI & two keys can be generated on the TTN console 
#ifndef RADIOLIB_LORAWAN_DEV_EUI   // Replace with your Device EUI
#define RADIOLIB_LORAWAN_DEV_EUI   0x0000000000000000
#endif
#ifndef RADIOLIB_LORAWAN_APP_KEY   // Replace with your App Key 
#define RADIOLIB_LORAWAN_APP_KEY   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#endif
#ifndef RADIOLIB_LORAWAN_NWK_KEY   // Put your Nwk Key here
#define RADIOLIB_LORAWAN_NWK_KEY   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
#endif

static const LoRaWANBand_t lorawan_region = EU868;
static const uint8_t lorawan_sub_band = 0;
static const uint64_t join_eui = RADIOLIB_LORAWAN_JOIN_EUI;
static const uint64_t dev_eui = RADIOLIB_LORAWAN_DEV_EUI;
static uint8_t app_key[] = { RADIOLIB_LORAWAN_APP_KEY };
static uint8_t nwk_key[] = { RADIOLIB_LORAWAN_NWK_KEY };

#endif
