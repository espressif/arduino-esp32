#include "esp_system.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_ESP_RMAKER_TASK_STACK && CONFIG_IDF_TARGET_ESP32

#include "RMakerType.h"

class Param
{
    private:
        const param_handle_t *param_handle;
       
    public:
        Param()
        {
            param_handle = NULL;
        }
        Param(const char *param_name, const char *param_type, param_val_t val, uint8_t properties)
        {
            param_handle = esp_rmaker_param_create(param_name, param_type, val, properties);
        }
        void setParamHandle(const param_handle_t *param_handle) 
        {
            this->param_handle = param_handle;
        }
        const char *getParamName()
        {
            return esp_rmaker_param_get_name(param_handle);
        }
        const param_handle_t *getParamHandle()
        {
            return param_handle;
        } 
         
        esp_err_t addUIType(const char *ui_type);
        esp_err_t addBounds(param_val_t min, param_val_t max, param_val_t step);
        esp_err_t updateAndReport(param_val_t val);
};

#endif