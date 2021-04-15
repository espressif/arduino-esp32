#include "RMakerParam.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_IDF_TARGET_ESP32

static esp_err_t err;

esp_err_t Param::addUIType(const char *ui_type)
{
    err = esp_rmaker_param_add_ui_type(param_handle, ui_type);
    if(err != ESP_OK) {
        log_e("Add UI type error");
    }
    return err;
}

esp_err_t Param::addBounds(param_val_t min, param_val_t max, param_val_t step)
{
    err = esp_rmaker_param_add_bounds(param_handle, min, max, step);
    if(err != ESP_OK) {
        log_e("Add Bounds error");
    }
    return err;
}

esp_err_t Param::updateAndReport(param_val_t val)
{
    err = esp_rmaker_param_update_and_report(getParamHandle(), val);
    if(err != ESP_OK){
        log_e("Update and Report param failed");
    }
    return err;
}

#endif