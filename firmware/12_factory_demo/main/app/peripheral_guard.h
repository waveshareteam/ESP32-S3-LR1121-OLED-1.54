/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PERIPHERAL_GUARD_H
#define PERIPHERAL_GUARD_H

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// Serializes shared SPI/I2C peripheral access from the main loop and tasks.
extern SemaphoreHandle_t peripheral_mutex;

bool peripheral_guard_init(void);
void peripheral_lock(void);
void peripheral_unlock(void);

#ifdef __cplusplus
}
#endif

#endif
