/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Arduino.h"
#include "esp_diagnostics.h"
class DiagnosticsClass
{
    public:
        esp_err_t initLogHook(esp_diag_log_write_cb_t write_cb, void *cb_arg = NULL);
        void enableLogHook(uint32_t type);
        void disableLogHook(uint32_t type);
};

extern DiagnosticsClass Diagnostics;
#endif