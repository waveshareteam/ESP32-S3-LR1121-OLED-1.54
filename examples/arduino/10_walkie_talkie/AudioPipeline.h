/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>

bool initAudioHardware();
bool startAudioTasks();
int audioStartupError();
void logAudioDiagnostics();
void clearAudioBuffers();
void prepareAudioSleep();
void resumeAudioFromSleep();
bool audioPopTxPacket(uint8_t *packet, size_t packetSize);
void audioPushRxPacket(const uint8_t *packet, size_t packetSize);
size_t audioTxQueuedBytes();
uint32_t audioTxEncodedFrames();
uint32_t audioMicReadFailures();
uint8_t audioSelectedMicSlot();
