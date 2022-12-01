#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Insights.h"
#include "esp_insights.h"
#include "esp_rmaker_utils.h"
static esp_err_t err;

esp_err_t InsightsClass::init(const char *auth_key, uint32_t log_type, const char *node_id, bool alloc_ext_ram)
{
    esp_rmaker_time_sync_init(NULL);
    this->config = {
        .log_type = log_type,
        .node_id = node_id,
        .auth_key = auth_key,
        .alloc_ext_ram = alloc_ext_ram
    };
    err = esp_insights_init(&config);
    if (err != ESP_OK) {
        log_e("Failed to initialize ESP Insights, err:0x%x", err);
    }
    else {
        log_i("=========================================");
        log_i("Insights enabled for Node ID %s", esp_insights_get_node_id());
        log_i("=========================================");
    }
    return err;
}

esp_err_t InsightsClass::enable(const char *auth_key, uint32_t log_type, const char *node_id, bool alloc_ext_ram)
{
    this->config = {
        .log_type = log_type,
        .node_id = node_id,
        .auth_key = auth_key,
        .alloc_ext_ram = alloc_ext_ram
    };
    err = esp_insights_enable(&config);
    if (err != ESP_OK) {
        log_e("Failed to enable ESP Insights, err:0x%x", err);
    }
    else {
        log_i("=========================================");
        log_i("Insights enabled for Node ID %s", esp_insights_get_node_id());
        log_i("=========================================");
    }
    return err;
}

esp_err_t InsightsClass::registerTransport(esp_insights_transport_config_t *config)
{
    err = esp_insights_transport_register(config);
    if (err != ESP_OK) {
        log_e("Failed to register transport, err:0x%x", err);
    }
    return err;
}

esp_err_t InsightsClass::sendData()
{
    return esp_insights_send_data();   
}

void InsightsClass::deinit()
{
    esp_insights_deinit();
}

void InsightsClass::disable()
{
    esp_insights_disable();
}

void InsightsClass::unregisterTransport()
{
    esp_insights_transport_unregister();
}

InsightsClass Insights;
#endif