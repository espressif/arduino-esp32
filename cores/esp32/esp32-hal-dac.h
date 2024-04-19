/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/soc_caps.h"
#if SOC_DAC_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

bool dacWrite(uint8_t pin, uint8_t value);
bool dacDisable(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* SOC_DAC_SUPPORTED */
