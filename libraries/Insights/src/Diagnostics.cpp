/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Diagnostics.h"
#include "esp_diagnostics.h"

static esp_err_t err;

esp_err_t DiagnosticsClass::initLogHook(esp_diag_log_write_cb_t write_cb, void *cb_arg)
{
    esp_diag_log_config_t config = {
        .write_cb = write_cb,
        .cb_arg = cb_arg,
    };
    err = esp_diag_log_hook_init(&config);
    if(err != ESP_OK) {
        log_e("Failed to initialize Log hook, err:0x%x", err);
    }
    return err;
}

void DiagnosticsClass::enableLogHook(uint32_t type)
{
    esp_diag_log_hook_enable(type);
}

void DiagnosticsClass::disableLogHook(uint32_t type)
{
    esp_diag_log_hook_disable(type);
}

DiagnosticsClass Diagnostics;
#endif
