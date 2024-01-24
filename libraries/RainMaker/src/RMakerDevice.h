// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK
#include "esp_system.h"
#include "RMakerParam.h"
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_params.h>

class Device
{
    public:
    typedef void (*deviceWriteCb)(Device*, Param*, const param_val_t val, void *priv_data, write_ctx_t *ctx);
    typedef void (*deviceReadCb)(Device*, Param*, void *priv_data, read_ctx_t *ctx);
    typedef struct {
        void *priv_data;
        deviceWriteCb write_cb;
        deviceReadCb read_cb;
    } RMakerDevicePrivT;
    private:
        const device_handle_t *device_handle;
        RMakerDevicePrivT private_data;

    protected:
        void setPrivateData(void *priv_data) {
            this->private_data.priv_data = priv_data;
        }
        
        const RMakerDevicePrivT* getDevicePrivateData()
        {
            return &this->private_data;
        }
    public:
        Device()
        {
            device_handle = NULL;
            this->private_data.priv_data = NULL;
            this->private_data.write_cb = NULL;
            this->private_data.read_cb = NULL;
        }

        Device(const char *dev_name, const char *dev_type = NULL, void *priv_data = NULL)
        {
            this->private_data.priv_data = priv_data;
            this->private_data.write_cb = NULL;
            this->private_data.read_cb = NULL;
            device_handle = esp_rmaker_device_create(dev_name, dev_type, &this->private_data);
            if(device_handle == NULL){
                log_e("Device create error");
            }   
        }
        void setDeviceHandle(const esp_rmaker_device_t *device_handle)
        {
            this->device_handle = device_handle;
        }
        const char *getDeviceName()
        {
            return esp_rmaker_device_get_name(device_handle);
        }
        const esp_rmaker_device_t *getDeviceHandle()
        {
            return device_handle;
        }

        esp_err_t deleteDevice();
        void addCb(deviceWriteCb write_cb, deviceReadCb read_cb = NULL);
        esp_err_t addDeviceAttr(const char *attr_name, const char *val);
        param_handle_t *getParamByName(const char *param_name);
        esp_err_t assignPrimaryParam(param_handle_t *param);
         
        //Generic Device Parameter
        esp_err_t addParam(Param parameter);

        //Standard Device Parameter
        esp_err_t addNameParam(const char *param_name = ESP_RMAKER_DEF_NAME_PARAM);
        esp_err_t addPowerParam(bool val, const char *param_name = ESP_RMAKER_DEF_POWER_NAME);
        esp_err_t addBrightnessParam(int val, const char *param_name = ESP_RMAKER_DEF_BRIGHTNESS_NAME);
        esp_err_t addHueParam(int val, const char *param_name = ESP_RMAKER_DEF_HUE_NAME);
        esp_err_t addSaturationParam(int val, const char *param_name = ESP_RMAKER_DEF_SATURATION_NAME);
        esp_err_t addIntensityParam(int val, const char *param_name = ESP_RMAKER_DEF_INTENSITY_NAME);
        esp_err_t addCCTParam(int val, const char *param_name = ESP_RMAKER_DEF_CCT_NAME);
        esp_err_t addDirectionParam(int val, const char *param_name = ESP_RMAKER_DEF_DIRECTION_NAME);
        esp_err_t addSpeedParam(int val, const char *param_name = ESP_RMAKER_DEF_SPEED_NAME);
        esp_err_t addTemperatureParam(float val, const char *param_name = ESP_RMAKER_DEF_TEMPERATURE_NAME);
          
        //Update Parameter
        esp_err_t updateAndReportParam(const char *param_name, bool val);
        esp_err_t updateAndReportParam(const char *param_name, int  val);
        esp_err_t updateAndReportParam(const char *param_name, float val);
        esp_err_t updateAndReportParam(const char *param_name, const char *val);

};

class Switch : public Device
{
    public:
        Switch()
        {
            standardSwitchDevice("Switch", NULL, true);
        }
        Switch(const char *dev_name, void *priv_data = NULL, bool power = true)
        {
            standardSwitchDevice(dev_name, priv_data, power);
        }
        void standardSwitchDevice(const char *dev_name, void *priv_data, bool power)
        {
            this->setPrivateData(priv_data);
            esp_rmaker_device_t *dev_handle = esp_rmaker_switch_device_create(dev_name, (void *)this->getDevicePrivateData(), power);
            setDeviceHandle(dev_handle);
            if(dev_handle == NULL){
                log_e("Switch device not created");
            }   
        } 
};

class LightBulb : public Device
{
    public:
        LightBulb()
        {
            standardLightBulbDevice("Light", NULL, true);
        }
        LightBulb(const char *dev_name, void *priv_data = NULL, bool power = true)
        {
            standardLightBulbDevice(dev_name, priv_data, power);
        }
        void standardLightBulbDevice(const char *dev_name, void *priv_data, bool power)
        {
            this->setPrivateData(priv_data);
            esp_rmaker_device_t *dev_handle = esp_rmaker_lightbulb_device_create(dev_name, (void *)this->getDevicePrivateData(), power);
            setDeviceHandle(dev_handle);
            if(dev_handle == NULL){
                log_e("Light device not created");
            }   
        }  
};       

class Fan : public Device
{
    public:
        Fan()
        {
            standardFanDevice("Fan", NULL, true);
        }
        Fan(const char *dev_name, void *priv_data = NULL, bool power = true)
        {
            standardFanDevice(dev_name, priv_data, power);
        }
        void standardFanDevice(const char *dev_name, void *priv_data, bool power)
        {
            esp_rmaker_device_t *dev_handle = esp_rmaker_fan_device_create(dev_name, priv_data, power);
            setDeviceHandle(dev_handle);
            if(dev_handle == NULL){
                log_e("Fan device not created");
            }
        } 
};

class TemperatureSensor : public Device
{
    public:
        TemperatureSensor()
        {
            standardTemperatureSensorDevice("Temperature-Sensor", NULL, 25.0);
        }
        TemperatureSensor(const char *dev_name, void *priv_data = NULL, float temp = 25.0)
        {
            standardTemperatureSensorDevice(dev_name, priv_data, temp);
        }
        void standardTemperatureSensorDevice(const char *dev_name, void *priv_data, float temp)
        {
            this->setPrivateData(priv_data);
            esp_rmaker_device_t *dev_handle = esp_rmaker_temp_sensor_device_create(dev_name, (void *)this->getDevicePrivateData(), temp);
            setDeviceHandle(dev_handle);
            if(dev_handle == NULL){
                log_e("Temperature Sensor device not created");
            }
        }
};
#endif