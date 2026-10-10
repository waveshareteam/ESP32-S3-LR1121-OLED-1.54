/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RADIO_RUNTIME_H
#define RADIO_RUNTIME_H

#include <stdbool.h>

// ISR flag consumed by the main loop to finish TX or read RX packets safely.
extern volatile bool radio_irq_fired;

int radio_apply_current_config(bool start_receive);
void radio_restore_irq_action(void);

#endif
