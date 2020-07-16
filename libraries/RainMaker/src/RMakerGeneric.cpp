#include "RMakerGeneric.h"

#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_core.h>
#include <esp_err.h>
#include <esp32-hal.h>

//RMAKER MAIN API's
void RMakerGenericClass::rmakerInit(char *node_name, char *node_type)
{
    esp_rmaker_config_t rainmaker_cfg = {
        .info = {
            .name = node_name,
            .type = node_type,
        },
        .enable_time_sync = false,
    };
    esp_err_t err = esp_rmaker_init(&rainmaker_cfg);
    if (err != ESP_OK) {
        log_e("Could not initialise ESP RainMaker. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}

esp_err_t RMakerGenericClass::rmakerStart()
{
    return esp_rmaker_start();
}

esp_err_t RMakerGenericClass::rmakerStop()
{
    return esp_rmaker_stop();
}

//NODE
char * RMakerGenericClass::getNodeID()
{
    return esp_rmaker_get_node_id();
}

esp_rmaker_node_info_t* RMakerGenericClass::getNodeInfo()
{
    return esp_rmaker_get_node_info();
}

esp_err_t RMakerGenericClass::addNodeAttr(const char *attr_name, const char *val)
{
    return esp_rmaker_node_add_attribute(attr_name, val);
}

//DEVICE 
esp_err_t RMakerGenericClass::createDevice(const char *dev_name, const char *dev_type, DeviceParamCb cb, void *data)
{
    return esp_rmaker_create_device(dev_name, dev_type, cb, data);
}

esp_err_t RMakerGenericClass::deviceAddAttr(const char *dev_name, const char *attr_name, const char *val)
{
    return esp_rmaker_device_add_attribute(dev_name, attr_name, val);
}

esp_err_t RMakerGenericClass::deviceAddParam(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val, uint8_t properties)
{
    return esp_rmaker_device_add_param(dev_name, param_name, val, properties);
}

esp_err_t RMakerGenericClass::deviceAssignPrimaryParam(const char *dev_name, const char *param_name)
{
    return esp_rmaker_device_assign_primary_param(dev_name, param_name);
}

//Create Service
esp_err_t RMakerGenericClass::createService(const char *serv_name, const char *type, DeviceParamCb cb, void *priv_data)
{
    return esp_rmaker_create_service(serv_name, type, cb, priv_data);
}

esp_err_t RMakerGenericClass::serviceAddParam(const char *serv_name, const char *param_name, esp_rmaker_param_val_t val, uint8_t properties)
{
    return esp_rmaker_service_add_param(serv_name, param_name, val, properties);
}

//Device Parameter
esp_err_t RMakerGenericClass::deviceAddNameParam(const char *dev_name, const char *param_name)
{
    return esp_rmaker_device_add_name_param(dev_name, param_name);
}

esp_err_t RMakerGenericClass::deviceAddPowerParam(const char *dev_name, const char *param_name, bool val)
{
    return esp_rmaker_device_add_power_param(dev_name, param_name, val);
}

esp_err_t RMakerGenericClass::deviceAddBrightnessParam(const char *dev_name, const char *param_name, int val)
{
    return esp_rmaker_device_add_brightness_param(dev_name, param_name, val);
}

esp_err_t RMakerGenericClass::deviceAddHueParam(const char *dev_name, const char *param_name, int val)
{
    return esp_rmaker_device_add_hue_param(dev_name, param_name, val); 
}

esp_err_t RMakerGenericClass::deviceAddSaturationParam(const char *dev_name, const char *param_name, int val)
{
    return esp_rmaker_device_add_saturation_param(dev_name, param_name, val);
}

esp_err_t RMakerGenericClass::deviceAddIntensityParam(const char *dev_name, const char *param_name, int val)
{
    return esp_rmaker_device_add_intensity_param(dev_name, param_name, val);
}

esp_err_t RMakerGenericClass::deviceAddCctParam(const char *dev_name, const char *param_name, int val)
{
    return esp_rmaker_device_add_cct_param(dev_name, param_name, val);
}

esp_err_t RMakerGenericClass::deviceAddDirectionParam(const char *dev_name, const char *param_name, int val)
{
    return esp_rmaker_device_add_direction_param(dev_name, param_name, val);
}

esp_err_t RMakerGenericClass::deviceAddSpeedParam(const char *dev_name, const char *param_name, int val)
{
    return esp_rmaker_device_add_speed_param(dev_name, param_name, val);
}

esp_err_t deviceAddTempratureParam(const char *dev_name, const char *param_name, float val)
{
    return esp_rmaker_device_add_temperature_param(dev_name, param_name, val);
}

//Service parameter
esp_err_t RMakerGenericClass::serviceAddOTAStatusParam(const char *serv_name, const char *param_name)
{
    return esp_rmaker_service_add_ota_status_param(serv_name, param_name);
}

esp_err_t RMakerGenericClass::serviceAddOTAInfoParam(const char *serv_name, const char *param_name)
{
    return esp_rmaker_service_add_ota_info_param(serv_name, param_name);
}

esp_err_t RMakerGenericClass::serviceAddOTAUrlParam(const char *serv_name, const char *param_name)
{
    return esp_rmaker_service_add_ota_url_param(serv_name, param_name);
}

//update parameter
esp_err_t RMakerGenericClass::updateParam(const char *dev_name, const char *param_name, Param_t val)
{
    return esp_rmaker_update_param(dev_name, param_name, val);
}

esp_err_t RMakerGenericClass::paramAddUIType(const char *dev_name, const char *name, const char *ui_type)
{
    return esp_rmaker_param_add_ui_type(dev_name, name, ui_type);
}

esp_err_t RMakerGenericClass::paramAddBounds(const char *dev_name, const char *param_name, esp_rmaker_param_val_t min, esp_rmaker_param_val_t max, esp_rmaker_param_val_t step)
{
    return esp_rmaker_param_add_bounds(dev_name, param_name, min, max, step);
}

esp_err_t RMakerGenericClass::paramAddType(const char *dev_name, const char *param_name, const char* type)
{
    return esp_rmaker_param_add_type(dev_name, param_name, type);
}

