// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp32-hal-adc.h"

#if SOC_ADC_SUPPORTED
#include "esp32-hal.h"
#include "esp32-hal-periman.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"

static uint8_t __analogAttenuation = ADC_11db;
static uint8_t __analogWidth = SOC_ADC_RTC_MAX_BITWIDTH; 
static uint8_t __analogReturnedWidth = SOC_ADC_RTC_MAX_BITWIDTH;

adc_oneshot_unit_handle_t adc_handle[SOC_ADC_PERIPH_NUM];
adc_cali_handle_t adc_cali_handle[SOC_ADC_PERIPH_NUM];

static bool adcDetachBus(void * pin){
    adc_channel_t adc_channel;
    adc_unit_t adc_unit;
    uint8_t used_channels = 0;

    adc_oneshot_io_to_channel((int)(pin-1), &adc_unit, &adc_channel);
    for (uint8_t channel = 0; channel < SOC_ADC_CHANNEL_NUM(adc_unit); channel++){
        int io_pin;
        adc_oneshot_channel_to_io(adc_unit, channel, &io_pin);
        if(perimanGetPinBusType(io_pin) == ESP32_BUS_TYPE_ADC_ONESHOT){
            used_channels++;
        }
    }

    if(used_channels == 1){ //only 1 channel is used
        esp_err_t err = adc_oneshot_del_unit(adc_handle[adc_unit]);
        if(err != ESP_OK){
            return false;
        }
        adc_handle[adc_unit] = NULL;
    }
    return true;
}

esp_err_t __analogChannelConfig(adc_bitwidth_t width, adc_attenuation_t atten, int8_t pin){
    esp_err_t err = ESP_OK;
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = width,
        .atten = (atten & 3),
    };
    if(pin == -1){  //Reconfigure all used analog pins/channels
        for(int adc_unit = 0 ; adc_unit < SOC_ADC_PERIPH_NUM; adc_unit++){
            if(adc_handle[adc_unit] != NULL){
                for (uint8_t channel = 0; channel < SOC_ADC_CHANNEL_NUM(adc_unit); channel++){
                    int io_pin;
                    adc_oneshot_channel_to_io( adc_unit, channel, &io_pin);
                    if(perimanGetPinBusType(io_pin) == ESP32_BUS_TYPE_ADC_ONESHOT){
                        err = adc_oneshot_config_channel(adc_handle[adc_unit], channel, &config);
                        if(err != ESP_OK){
                            log_e("adc_oneshot_config_channel failed with error: %d", err);
                            return err;
                        }
                    }
                }
                //ADC calibration reconfig only if all channels are updated
                if(adc_cali_handle[adc_unit] != NULL){
                    #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
                        log_d("Deleting ADC_UNIT_%d cali handle",adc_unit);
                        err = adc_cali_delete_scheme_curve_fitting(adc_cali_handle[adc_unit]);
                        if(err != ESP_OK){
                            log_e("adc_cali_delete_scheme_curve_fitting failed with error: %d", err);
                            return err;
                        }
                        adc_cali_curve_fitting_config_t cali_config = {
                            .unit_id = adc_unit,
                            .atten = atten,
                            .bitwidth = width,
                        };
                        log_d("Creating ADC_UNIT_%d curve cali handle",adc_unit);
                        err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle[adc_unit]);
                        if(err != ESP_OK){
                            log_e("adc_cali_create_scheme_curve_fitting failed with error: %d", err);
                            return err;
                        }
                    #elif !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2) //ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
                        log_d("Deleting ADC_UNIT_%d line cali handle",adc_unit);
                        err = adc_cali_delete_scheme_line_fitting(adc_cali_handle[adc_unit]);
                        if(err != ESP_OK){
                            log_e("adc_cali_delete_scheme_line_fitting failed with error: %d", err);
                            return err;
                        }
                        adc_cali_line_fitting_config_t cali_config = {
                            .unit_id = adc_unit,
                            .atten = atten,
                            .bitwidth = width,
                        };
                        log_d("Creating ADC_UNIT_%d line cali handle",adc_unit);
                        err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle[adc_unit]);
                        if(err != ESP_OK){
                            log_e("adc_cali_create_scheme_line_fitting failed with error: %d", err);
                            return err;
                        }
                    #endif
                }
            }
        }

        //make it default for next channels
        __analogWidth = width;
        __analogAttenuation = atten;
    }
    else{ //Reconfigure single channel
        if(perimanGetPinBusType(pin) == ESP32_BUS_TYPE_ADC_ONESHOT){
            adc_channel_t channel;
            adc_unit_t adc_unit;

            adc_oneshot_io_to_channel(pin, &adc_unit, &channel);
            if(err != ESP_OK){
                log_e("Pin %u is not ADC pin!", pin);
                return err;
            }
            err = adc_oneshot_config_channel(adc_handle[adc_unit], channel, &config);
            if(err != ESP_OK){
                log_e("adc_oneshot_config_channel failed with error: %d", err);
                return err;
            }
        }
        else {
            log_e("Pin is not configured as analog channel");
        }
    }
    return ESP_OK;
}

static inline uint16_t mapResolution(uint16_t value){
    uint8_t from = __analogWidth;
    if (from == __analogReturnedWidth){
        return value;
    }
    if (from > __analogReturnedWidth){
        return value >> (from  - __analogReturnedWidth);
    }
    return value << (__analogReturnedWidth - from);
}

void __analogSetAttenuation(adc_attenuation_t attenuation){
    if(__analogChannelConfig(__analogWidth, attenuation, -1) != ESP_OK){
        log_e("__analogChannelConfig failed!");
    }
}

#if CONFIG_IDF_TARGET_ESP32
void __analogSetWidth(uint8_t bits){
    if(bits < SOC_ADC_RTC_MIN_BITWIDTH){
        bits = SOC_ADC_RTC_MIN_BITWIDTH;
    } 
    else if(bits > SOC_ADC_RTC_MAX_BITWIDTH){
        bits = SOC_ADC_RTC_MAX_BITWIDTH;
    }
    if(__analogChannelConfig(bits, __analogAttenuation, -1) != ESP_OK){
        log_e("__analogChannelConfig failed!");
    }
}
#endif

esp_err_t __analogInit(uint8_t pin, adc_channel_t channel, adc_unit_t adc_unit){
    esp_err_t err = ESP_OK;
    if(adc_handle[adc_unit] == NULL) {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = adc_unit,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        err = adc_oneshot_new_unit(&init_config1, &adc_handle[adc_unit]);

        if(err != ESP_OK){
            log_e("adc_oneshot_new_unit failed with error: %d", err);
            return err;
        }
    }

    if(!perimanSetPinBus(pin, ESP32_BUS_TYPE_ADC_ONESHOT, (void *)(pin+1))){
        adcDetachBus((void *)(pin+1));
        return err;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = __analogWidth,
        .atten = __analogAttenuation,
    };

    err = adc_oneshot_config_channel(adc_handle[adc_unit], channel, &config);
    if(err != ESP_OK){
        log_e("adc_oneshot_config_channel failed with error: %d", err);
        return err;
    }
    perimanSetBusDeinit(ESP32_BUS_TYPE_ADC_ONESHOT, adcDetachBus);
    return ESP_OK;
}

void __analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation){
    if(__analogChannelConfig(__analogWidth, attenuation, pin) != ESP_OK)
    {
        log_e("__analogChannelConfig failed!");
    }
}

void __analogReadResolution(uint8_t bits){
    if(!bits || bits > 16){
        return;
    }
    __analogReturnedWidth = bits;
    
#if CONFIG_IDF_TARGET_ESP32
    __analogSetWidth(bits);         // hardware analog resolution from 9 to 12
#endif
}

uint16_t __analogRead(uint8_t pin){
    int value = 0;
    adc_channel_t channel;
    adc_unit_t adc_unit;

    esp_err_t err = ESP_OK;
    err = adc_oneshot_io_to_channel(pin, &adc_unit, &channel);
    if(err != ESP_OK){
        log_e("Pin %u is not ADC pin!", pin);
        return value;
    }

    if(perimanGetPinBus(pin, ESP32_BUS_TYPE_ADC_ONESHOT) == NULL){
        log_d("Calling __analogInit! pin = %d", pin);
        err = __analogInit(pin, channel, adc_unit);
        if(err != ESP_OK){
            log_e("Analog initialization failed!");
            return value;
        }
    }

    adc_oneshot_read(adc_handle[adc_unit], channel, &value);
    return mapResolution(value);
}

uint32_t __analogReadMilliVolts(uint8_t pin){
    int value = 0;
    adc_channel_t channel;
    adc_unit_t adc_unit;
    esp_err_t err = ESP_OK;

    adc_oneshot_io_to_channel(pin, &adc_unit, &channel);
    if(err != ESP_OK){
        log_e("Pin %u is not ADC pin!", pin);
        return value;
    }

    if(perimanGetPinBus(pin, ESP32_BUS_TYPE_ADC_ONESHOT) == NULL){
        err = __analogInit(pin, channel, adc_unit);
        if(err != ESP_OK){
            log_e("Analog initialization failed!");
            return value;
        }
    }

    if(adc_cali_handle[adc_unit] == NULL){
        log_d("Creating cali handle for ADC_%d", adc_unit);
        #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = adc_unit,
                .atten = __analogAttenuation,
                .bitwidth = __analogWidth,
            };
            err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle[adc_unit]);
        #elif !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2) //ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = adc_unit,
                .bitwidth = __analogWidth,
                .atten = __analogAttenuation,
            };
            err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle[adc_unit]);
        #endif
        if(err != ESP_OK){
            log_e("adc_cali_create_scheme_x failed!");
            return value;
        }
    }

    err = adc_oneshot_get_calibrated_result(adc_handle[adc_unit], adc_cali_handle[adc_unit], channel, &value);
    if(err != ESP_OK){
        log_e("adc_oneshot_get_calibrated_result failed!");
        return 0;
    }
    return value;
}

extern uint16_t analogRead(uint8_t pin) __attribute__ ((weak, alias("__analogRead")));
extern uint32_t analogReadMilliVolts(uint8_t pin) __attribute__ ((weak, alias("__analogReadMilliVolts")));
extern void analogReadResolution(uint8_t bits) __attribute__ ((weak, alias("__analogReadResolution")));
extern void analogSetAttenuation(adc_attenuation_t attenuation) __attribute__ ((weak, alias("__analogSetAttenuation")));
extern void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation) __attribute__ ((weak, alias("__analogSetPinAttenuation")));

#if CONFIG_IDF_TARGET_ESP32
extern void analogSetWidth(uint8_t bits) __attribute__ ((weak, alias("__analogSetWidth")));
#endif

#endif
