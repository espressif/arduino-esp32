/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

void printBeforeSetupInfo(void);
void printAfterSetupInfo(void);

// Flash frequency runtime detection
uint32_t getFlashFrequencyMHz(void);
uint8_t getFlashSourceFrequencyMHz(void);
uint8_t getFlashClockDivider(void);
bool isFlashHighPerformanceModeEnabled(void);
