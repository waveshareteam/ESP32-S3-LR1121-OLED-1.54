/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "peripheral_guard.h"

// Shared guard for radio, RTC, battery gauge and display-side I2C operations.
SemaphoreHandle_t peripheral_mutex = NULL;

bool peripheral_guard_init(void)
{
    peripheral_mutex = xSemaphoreCreateMutex();
    return peripheral_mutex != NULL;
}

void peripheral_lock(void)
{
    if (peripheral_mutex) {
        xSemaphoreTake(peripheral_mutex, portMAX_DELAY);
    }
}

void peripheral_unlock(void)
{
    if (peripheral_mutex) {
        xSemaphoreGive(peripheral_mutex);
    }
}
