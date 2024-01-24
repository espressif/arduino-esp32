#include "sdkconfig.h"
#ifdef CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK
#include "RMakerParam.h"

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

esp_err_t Param::addValidStrList(const char **string_list, uint8_t count) {
    esp_err_t err = esp_rmaker_param_add_valid_str_list(getParamHandle(), string_list, count);
    if (err != ESP_OK) {
        log_e("Add valid string list error");
    }
    return err;
}
#endif
