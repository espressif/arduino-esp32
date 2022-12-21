/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "esp_system.h"
#include "Arduino.h"
#include "esp_diagnostics_metrics.h"
#include "esp_diagnostics.h"

class MetricsClass
{
    public:
        esp_err_t init(esp_diag_metrics_config_t *config);
        esp_err_t registerMetric(const char *tag, const char *key, const char *label, const char *path, esp_diag_data_type_t type);
        esp_err_t unregister(const char *key);
        esp_err_t unregisterAll();
        esp_err_t add(esp_diag_data_type_t data_type, const char *key, const void *val, size_t val_sz, uint64_t ts);
        esp_err_t addBool(const char *key, bool b);
        esp_err_t addInt(const char *key, int32_t i);
        esp_err_t addUint(const char *key, uint32_t u);
        esp_err_t addFloat(const char *key, float f);
        esp_err_t addIPv4(const char *key, uint32_t ip);
        esp_err_t addMAC(const char *key, uint8_t *mac);
        esp_err_t addString(const char *key, const char *str);
        esp_err_t initHeapMetrics();
        esp_err_t dumpHeapMetrics();
        esp_err_t initWiFiMetrics();
        esp_err_t dumpWiFiMetrics();
        esp_err_t deinit();
        void resetHeapMetricsInterval(uint32_t period);
        void resetWiFiMetricsInterval(uint32_t period);
        esp_err_t deinitHeapMetrics();
        esp_err_t deinitWiFiMetrics();
};

extern MetricsClass Metrics;
#endif
