/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Variables.h"
#include "esp_diagnostics.h"
#include "esp_diagnostics_variables.h"
#include "esp_diagnostics_network_variables.h"

static esp_err_t err;

esp_err_t VariablesClass::init(esp_diag_variable_config_t *config)
{
    return esp_diag_variable_init(config);
}

esp_err_t VariablesClass::registerVariable(const char *tag, const char *key, const char *label, const char *path, esp_diag_data_type_t type)
{
    err = esp_diag_variable_register(tag, key, label, path, type);
    if(err != ESP_OK) {
        log_e("Failed to register variable. key: %s, err:0x%x", key, err);
    }
    return err;
}

esp_err_t VariablesClass::unregister(const char *key)
{
    err = esp_diag_variable_unregister(key);
    if(err != ESP_OK) {
        log_e("Failed to unregister variable. key: %s, err:0x%x", key, err);
    }
    return err;
}

esp_err_t VariablesClass::unregisterAll()
{
    return esp_diag_variable_unregister_all();
}

esp_err_t VariablesClass::add(esp_diag_data_type_t data_type, const char *key, const void *val, size_t val_sz, uint64_t ts)
{
    return esp_diag_variable_add(data_type, key, val, val_sz, ts);
}

esp_err_t VariablesClass::addBool(const char *key, bool b)
{
    return esp_diag_variable_add_bool(key, b);
}

esp_err_t VariablesClass::addInt(const char *key, int32_t i)
{
    return esp_diag_variable_add_int(key, i);    
}

esp_err_t VariablesClass::addUint(const char *key, uint32_t u)
{
    return esp_diag_variable_add_uint(key, u);
}

esp_err_t VariablesClass::addFloat(const char *key, float f)
{
    return esp_diag_variable_add_float(key, f);    
}

esp_err_t VariablesClass::addIPv4(const char *key, uint32_t ip)
{
    return esp_diag_variable_add_ipv4(key, ip);
}

esp_err_t VariablesClass::addMAC(const char *key, uint8_t *mac)
{
    return esp_diag_variable_add_mac(key, mac);
}

esp_err_t VariablesClass::addString(const char *key, const char *str)
{
    return esp_diag_variable_add_str(key, str);    
}

esp_err_t VariablesClass::deinit()
{
    return esp_diag_variables_deinit();
}

esp_err_t VariablesClass::initNetworkVariables()
{
    err = esp_diag_network_variables_init();
    if(err != ESP_OK) {
        log_e("Failed to initialize network variables, err=0x%x", err);
    }
    return err;
}

esp_err_t VariablesClass::deinitNetworkVariables()
{
    err = esp_diag_network_variables_deinit();
    if(err != ESP_OK) {
        log_e("Failed to deinitialize network variables, err=0x%x", err);
    }
    return err;
}

VariablesClass Variables;
#endif