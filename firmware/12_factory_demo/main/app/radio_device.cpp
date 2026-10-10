/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "radio_device.h"

// RadioLib keeps references to HAL and Module, so keep them alive globally.
EspHal *hal = new EspHal();
LR1121 radio = new Module(hal, BSP_SPI_CS, BSP_SPI_INT, BSP_SPI_RESET, BSP_SPI_BUSY);
