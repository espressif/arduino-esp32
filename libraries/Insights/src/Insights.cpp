/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Insights.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "esp_insights.h"
#include "esp_diagnostics.h"
#include "esp_diagnostics_metrics.h"
#include "esp_diagnostics_system_metrics.h"
#include "esp_diagnostics_variables.h"
#include "esp_diagnostics_network_variables.h"

const char * ERROR_INSIGHTS_NOT_INIT = "ESP Insights not initialized";

#define BOOL_FN_OR_ERROR(f,e) \
    if(!initialized){ \
        log_e("%s",ERROR_INSIGHTS_NOT_INIT); \
        return false; \
    } \
    esp_err_t err = f; \
    if (err != ESP_OK) { \
        log_e("ESP Insights " e ", err:0x%x", err); \
    } \
    return err == ESP_OK;

#define BOOL_FN_OR_ERROR_ARG(f,e,a) \
    if(!initialized){ \
        log_e("%s",ERROR_INSIGHTS_NOT_INIT); \
        return false; \
    } \
    esp_err_t err = f; \
    if (err != ESP_OK) { \
        log_e("ESP Insights " e ", err:0x%x", a, err); \
    } \
    return err == ESP_OK;

#define VOID_FN_OR_ERROR(f) \
    if(!initialized){ \
        log_e("%s",ERROR_INSIGHTS_NOT_INIT); \
        return; \
    } \
    f;

ESPInsightsClass::ESPInsightsClass(): initialized(false){}

ESPInsightsClass::~ESPInsightsClass(){
    end();
}

bool ESPInsightsClass::begin(const char *auth_key, const char *node_id, uint32_t log_type, bool alloc_ext_ram, bool use_default_transport){
    if(!initialized){
        if(log_type == 0xFFFFFFFF){
            log_type = (ESP_DIAG_LOG_TYPE_ERROR | ESP_DIAG_LOG_TYPE_WARNING | ESP_DIAG_LOG_TYPE_EVENT);
        }
        esp_insights_config_t config = {.log_type = log_type, .node_id = node_id, .auth_key = auth_key, .alloc_ext_ram = alloc_ext_ram};
        esp_err_t err = ESP_OK;
        if (use_default_transport) {
            err = esp_insights_init(&config);
        } else {
            err = esp_insights_enable(&config);
        }
        if (err != ESP_OK) {
            log_e("Failed to initialize ESP Insights, err:0x%x", err);
        }
        initialized = err == ESP_OK;
        metrics.setInitialized(initialized);
        variables.setInitialized(initialized);
    } else {
        log_i("ESP Insights already initialized");
    }
    return initialized;
}

void ESPInsightsClass::end(){
    if(initialized){
        esp_insights_deinit();
        initialized = false;
        metrics.setInitialized(initialized);
        variables.setInitialized(initialized);
    }
}

const char * ESPInsightsClass::nodeID(){
    if(!initialized){
        log_e("%s",ERROR_INSIGHTS_NOT_INIT);
        return "";
    }
    return esp_insights_get_node_id();
}

bool ESPInsightsClass::event(const char *tag, const char *format, ...){
    if(!initialized){
        log_e("%s",ERROR_INSIGHTS_NOT_INIT);
        return false;
    }

    char loc_buf[64];
    char * temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if(len < 0) {
        va_end(arg);
        return false;
    };
    if(len >= (int)sizeof(loc_buf)){  // comparation of same sign type for the compiler
        temp = (char*) malloc(len+1);
        if(temp == NULL) {
            va_end(arg);
            return false;
        }
        len = vsnprintf(temp, len+1, format, arg);
    }
    va_end(arg);
    esp_err_t err = esp_diag_log_event(tag, "%s", temp);
    if(temp != loc_buf){
        free(temp);
    }
    if (err != ESP_OK) {
        log_e("Failed to send ESP Insights event, err:0x%x", err);
    }
    return err == ESP_OK;
}

bool ESPInsightsClass::send(){
    BOOL_FN_OR_ERROR(esp_insights_send_data(),"Failed to send");
}

void ESPInsightsClass::dumpTasksStatus(){
    VOID_FN_OR_ERROR(esp_diag_task_snapshot_dump());
}

// ESPInsightsMetricsClass

bool ESPInsightsMetricsClass::addBool(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_BOOL),"Failed to add metric '%s'",key);
}

bool ESPInsightsMetricsClass::addInt(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_INT),"Failed to add metric '%s'",key);
}

bool ESPInsightsMetricsClass::addUint(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_UINT),"Failed to add metric '%s'",key);
}

bool ESPInsightsMetricsClass::addFloat(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_FLOAT),"Failed to add metric '%s'",key);
}

bool ESPInsightsMetricsClass::addString(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_STR),"Failed to add metric '%s'",key);
}

bool ESPInsightsMetricsClass::addIPv4(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_IPv4),"Failed to add metric '%s'",key);
}

bool ESPInsightsMetricsClass::addMAC(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_MAC),"Failed to add metric '%s'",key);
}

bool ESPInsightsMetricsClass::setBool(const char *key, bool b){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_add_bool(key, b),"Failed to set metric '%s'",key);
}

bool ESPInsightsMetricsClass::setInt(const char *key, int32_t i){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_add_int(key, i),"Failed to set metric '%s'",key);
}

bool ESPInsightsMetricsClass::setUint(const char *key, uint32_t u){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_add_uint(key, u),"Failed to set metric '%s'",key);
}

bool ESPInsightsMetricsClass::setFloat(const char *key, float f){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_add_float(key, f),"Failed to set metric '%s'",key);
}

bool ESPInsightsMetricsClass::setString(const char *key, const char *str){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_add_str(key, str),"Failed to set metric '%s'",key);
}

bool ESPInsightsMetricsClass::setIPv4(const char *key, uint32_t ip){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_add_ipv4(key, ip),"Failed to set metric '%s'",key);
}

bool ESPInsightsMetricsClass::setMAC(const char *key, uint8_t *mac){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_add_mac(key, mac),"Failed to set metric '%s'",key);
}

bool ESPInsightsMetricsClass::remove(const char *key){
    BOOL_FN_OR_ERROR_ARG(esp_diag_metrics_unregister(key),"Failed to remove metric '%s'",key);
}

bool ESPInsightsMetricsClass::removeAll(){
    BOOL_FN_OR_ERROR(esp_diag_metrics_unregister_all(),"Failed to remove metrics");
}

void ESPInsightsMetricsClass::setHeapPeriod(uint32_t seconds){
    VOID_FN_OR_ERROR(esp_diag_heap_metrics_reset_interval(seconds));
}

bool ESPInsightsMetricsClass::dumpHeap(){
    BOOL_FN_OR_ERROR(esp_diag_heap_metrics_dump(),"Failed to send heap metrics");
}

void ESPInsightsMetricsClass::setWiFiPeriod(uint32_t seconds){
    VOID_FN_OR_ERROR(esp_diag_wifi_metrics_reset_interval(seconds));
}

bool ESPInsightsMetricsClass::dumpWiFi(){
    BOOL_FN_OR_ERROR(esp_diag_wifi_metrics_dump(),"Failed to send wifi metrics");
}

// ESPInsightsVariablesClass

bool ESPInsightsVariablesClass::addBool(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_BOOL),"Failed to add variable '%s'",key);
}

bool ESPInsightsVariablesClass::addInt(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_INT),"Failed to add variable '%s'",key);
}

bool ESPInsightsVariablesClass::addUint(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_UINT),"Failed to add variable '%s'",key);
}

bool ESPInsightsVariablesClass::addFloat(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_FLOAT),"Failed to add variable '%s'",key);
}

bool ESPInsightsVariablesClass::addString(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_STR),"Failed to add variable '%s'",key);
}

bool ESPInsightsVariablesClass::addIPv4(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_IPv4),"Failed to add variable '%s'",key);
}

bool ESPInsightsVariablesClass::addMAC(const char *tag, const char *key, const char *label, const char *path){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_register(tag, key, label, path, ESP_DIAG_DATA_TYPE_MAC),"Failed to add variable '%s'",key);
}

bool ESPInsightsVariablesClass::setBool(const char *key, bool b){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_add_bool(key, b),"Failed to set variable '%s'",key);
}

bool ESPInsightsVariablesClass::setInt(const char *key, int32_t i){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_add_int(key, i),"Failed to set variable '%s'",key);
}

bool ESPInsightsVariablesClass::setUint(const char *key, uint32_t u){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_add_uint(key, u),"Failed to set variable '%s'",key);
}

bool ESPInsightsVariablesClass::setFloat(const char *key, float f){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_add_float(key, f),"Failed to set variable '%s'",key);
}

bool ESPInsightsVariablesClass::setString(const char *key, const char *str){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_add_str(key, str),"Failed to set variable '%s'",key);
}

bool ESPInsightsVariablesClass::setIPv4(const char *key, uint32_t ip){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_add_ipv4(key, ip),"Failed to set variable '%s'",key);
}

bool ESPInsightsVariablesClass::setMAC(const char *key, uint8_t *mac){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_add_mac(key, mac),"Failed to set variable '%s'",key);
}

bool ESPInsightsVariablesClass::remove(const char *key){
    BOOL_FN_OR_ERROR_ARG(esp_diag_variable_unregister(key),"Failed to remove variable '%s'",key);
}

bool ESPInsightsVariablesClass::removeAll(){
    BOOL_FN_OR_ERROR(esp_diag_variable_unregister_all(),"Failed to remove variables");
}

ESPInsightsClass Insights;

#endif
