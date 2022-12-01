#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Metrics.h"
#include "esp_diagnostics_metrics.h"
#include "esp_diagnostics_system_metrics.h"

static esp_err_t err;

esp_err_t MetricsClass::init(esp_diag_metrics_config_t *config)
{
    return esp_diag_metrics_init(config);
}

esp_err_t MetricsClass::registerMetric(const char *tag, const char *key, const char *label, const char *path, esp_diag_data_type_t type)
{
    err = esp_diag_metrics_register(tag, key, label, path, type);
    if(err != ESP_OK) {
        log_e("Failed to register metric. key: %s, err:0x%x", key, err);
    }
    return err;
}

esp_err_t MetricsClass::unregister(const char *key)
{
    err = esp_diag_metrics_unregister(key);
    if(err != ESP_OK) {
        log_e("Failed to unregister metric. key: %s, err:0x%x", key, err);
    }
    return err;
}

esp_err_t MetricsClass::unregisterAll()
{
    return esp_diag_metrics_unregister_all();
}

esp_err_t MetricsClass::add(esp_diag_data_type_t data_type, const char *key, const void *val, size_t val_sz, uint64_t ts)
{
    return esp_diag_metrics_add(data_type, key, val, val_sz, ts);
}

esp_err_t MetricsClass::addBool(const char *key, bool b)
{
    return esp_diag_metrics_add_bool(key, b);
}

esp_err_t MetricsClass::addInt(const char *key, int32_t i)
{
    return esp_diag_metrics_add_int(key, i);    
}

esp_err_t MetricsClass::addUint(const char *key, uint32_t u)
{
    return esp_diag_metrics_add_uint(key, u);
}

esp_err_t MetricsClass::addFloat(const char *key, float f)
{
    return esp_diag_metrics_add_float(key, f);    
}

esp_err_t MetricsClass::addIPv4(const char *key, uint32_t ip)
{
    return esp_diag_metrics_add_ipv4(key, ip);
}

esp_err_t MetricsClass::addMAC(const char *key, uint8_t *mac)
{
    return esp_diag_metrics_add_mac(key, mac);
}

esp_err_t MetricsClass::addString(const char *key, const char *str)
{
    return esp_diag_metrics_add_str(key, str);    
}

esp_err_t MetricsClass::initHeapMetrics()
{
    err = esp_diag_heap_metrics_init();
    if(err != ESP_OK) {
        log_e("Failed to initialize heap metrics, err=0x%x", err);
    }
    return err;
}

esp_err_t MetricsClass::dumpHeapMetrics()
{
    err = esp_diag_heap_metrics_dump();
    if(err != ESP_OK) {
        log_e("Failed to dump heap metrics, err=0x%x", err);
    }
    return err;
}

esp_err_t MetricsClass::initWiFiMetrics()
{
    err = esp_diag_wifi_metrics_init();
    if(err != ESP_OK) {
        log_e("Failed to initialize WiFi metrics, err=0x%x", err);
    }
    return err;   
}

esp_err_t MetricsClass::dumpWiFiMetrics()
{
    err = esp_diag_wifi_metrics_dump();
    if(err != ESP_OK) {
        log_e("Failed to dump heap metrics, err=0x%x", err);
    }
    return err;
}

esp_err_t MetricsClass::deinit()
{
    return esp_diag_metrics_deinit();
}

void MetricsClass::resetHeapMetricsInterval(uint32_t period)
{
    return esp_diag_heap_metrics_reset_interval(period);
}

void MetricsClass::resetWiFiMetricsInterval(uint32_t period)
{
    return esp_diag_wifi_metrics_reset_interval(period);
}

esp_err_t MetricsClass::deinitHeapMetrics()
{
    return esp_diag_heap_metrics_deinit();
}

esp_err_t MetricsClass::deinitWiFiMetrics()
{
    return esp_diag_wifi_metrics_deinit();
}

MetricsClass Metrics;
#endif