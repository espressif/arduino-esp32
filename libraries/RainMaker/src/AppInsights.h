/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "sdkconfig.h"
#include "Arduino.h"
#include <inttypes.h>

bool initAppInsights(uint32_t log_type = 0xffffffff, bool alloc_ext_ram = false);
