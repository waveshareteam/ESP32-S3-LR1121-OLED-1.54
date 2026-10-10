/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_config.h"
#include "esp_err.h"

// Audio pipeline owns Codec2, I2S playback and packet queues.
void audio_pipeline_start(void);
void audio_pipeline_clear_all(void);
esp_err_t audio_pipeline_prepare_sleep(void);
esp_err_t audio_pipeline_resume_from_sleep(void);
bool audio_pipeline_pop_tx_packet(uint8_t *packet, size_t packet_size);
void audio_pipeline_push_rx_packet(const uint8_t *packet, size_t packet_size);

#endif
