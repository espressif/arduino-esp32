/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Arduino.h"
#include "esp_system.h"
#include "esp_insights.h"

class InsightsClass
{
    private:
        esp_insights_config_t config = {.log_type = 0, .node_id = NULL, .auth_key = NULL, .alloc_ext_ram = false};
    public:
        esp_err_t init(const char *auth_key, uint32_t log_type, const char *node_id = NULL, bool alloc_ext_ram = false);
        esp_err_t enable(const char *auth_key, uint32_t log_type, const char *node_id = NULL, bool alloc_ext_ram = false);
        esp_err_t registerTransport(esp_insights_transport_config_t *config);
        esp_err_t sendData();
        void deinit();
        void disable();
        void unregisterTransport();
};

extern InsightsClass Insights;
#endif
