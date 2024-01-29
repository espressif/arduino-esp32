// Copyright 2015-2023 Espressif Systems (Shanghai) PTE LTD
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
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali_scheme.h"

// ESP32-C2 does not define those two for some reason
#ifndef SOC_ADC_DIGI_RESULT_BYTES
#define SOC_ADC_DIGI_RESULT_BYTES               (4)
#endif
#ifndef SOC_ADC_DIGI_DATA_BYTES_PER_CONV
#define SOC_ADC_DIGI_DATA_BYTES_PER_CONV        (4)
#endif

static uint8_t __analogAttenuation = ADC_11db;
static uint8_t __analogWidth = SOC_ADC_RTC_MAX_BITWIDTH; 
static uint8_t __analogReturnedWidth = SOC_ADC_RTC_MAX_BITWIDTH;

typedef struct {
    voidFuncPtr fn;
    void* arg;
} interrupt_config_t;

typedef struct {
    adc_oneshot_unit_handle_t adc_oneshot_handle;
    adc_continuous_handle_t adc_continuous_handle;
    interrupt_config_t adc_interrupt_handle;
    adc_cali_handle_t adc_cali_handle;
    uint32_t buffer_size;
    uint32_t conversion_frame_size;
} adc_handle_t;

adc_handle_t adc_handle[SOC_ADC_PERIPH_NUM];

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
        esp_err_t err = adc_oneshot_del_unit(adc_handle[adc_unit].adc_oneshot_handle);
        if(err != ESP_OK){
            return false;
        }
        adc_handle[adc_unit].adc_oneshot_handle = NULL;
        if(adc_handle[adc_unit].adc_cali_handle != NULL){
    #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED        
            err = adc_cali_delete_scheme_curve_fitting(adc_handle[adc_unit].adc_cali_handle);
            if(err != ESP_OK){
                return false;
            }
    #elif !defined(CONFIG_IDF_TARGET_ESP32H2)
            err = adc_cali_delete_scheme_line_fitting(adc_handle[adc_unit].adc_cali_handle);
            if(err != ESP_OK){
                return false;
            }
    #endif
        }
        adc_handle[adc_unit].adc_cali_handle = NULL;
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
            if(adc_handle[adc_unit].adc_oneshot_handle != NULL){
                for (uint8_t channel = 0; channel < SOC_ADC_CHANNEL_NUM(adc_unit); channel++){
                    int io_pin;
                    adc_oneshot_channel_to_io( adc_unit, channel, &io_pin);
                    if(perimanGetPinBusType(io_pin) == ESP32_BUS_TYPE_ADC_ONESHOT){
                        err = adc_oneshot_config_channel(adc_handle[adc_unit].adc_oneshot_handle, channel, &config);
                        if(err != ESP_OK){
                            log_e("adc_oneshot_config_channel failed with error: %d", err);
                            return err;
                        }
                    }
                }
                //ADC calibration reconfig only if all channels are updated
                if(adc_handle[adc_unit].adc_cali_handle != NULL){
                    #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
                        log_d("Deleting ADC_UNIT_%d cali handle",adc_unit);
                        err = adc_cali_delete_scheme_curve_fitting(adc_handle[adc_unit].adc_cali_handle);
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
                        err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_handle[adc_unit].adc_cali_handle);
                        if(err != ESP_OK){
                            log_e("adc_cali_create_scheme_curve_fitting failed with error: %d", err);
                            return err;
                        }
                    #elif !defined(CONFIG_IDF_TARGET_ESP32H2) //ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
                        log_d("Deleting ADC_UNIT_%d line cali handle",adc_unit);
                        err = adc_cali_delete_scheme_line_fitting(adc_handle[adc_unit].adc_cali_handle);
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
                        err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_handle[adc_unit].adc_cali_handle);
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
            err = adc_oneshot_config_channel(adc_handle[adc_unit].adc_oneshot_handle, channel, &config);
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
    if(adc_handle[adc_unit].adc_oneshot_handle == NULL) {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = adc_unit,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        err = adc_oneshot_new_unit(&init_config1, &adc_handle[adc_unit].adc_oneshot_handle);

        if(err != ESP_OK){
            log_e("adc_oneshot_new_unit failed with error: %d", err);
            return err;
        }
    }
    perimanSetBusDeinit(ESP32_BUS_TYPE_ADC_ONESHOT, adcDetachBus);

    if(!perimanSetPinBus(pin, ESP32_BUS_TYPE_ADC_ONESHOT, (void *)(pin+1), adc_unit, channel)){
        adcDetachBus((void *)(pin+1));
        return err;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = __analogWidth,
        .atten = __analogAttenuation,
    };

    err = adc_oneshot_config_channel(adc_handle[adc_unit].adc_oneshot_handle, channel, &config);
    if(err != ESP_OK){
        log_e("adc_oneshot_config_channel failed with error: %d", err);
        return err;
    }
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

    adc_oneshot_read(adc_handle[adc_unit].adc_oneshot_handle, channel, &value);
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

    if(adc_handle[adc_unit].adc_cali_handle == NULL){
        log_d("Creating cali handle for ADC_%d", adc_unit);
        #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = adc_unit,
                .atten = __analogAttenuation,
                .bitwidth = __analogWidth,
            };
            err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_handle[adc_unit].adc_cali_handle);
        #elif !defined(CONFIG_IDF_TARGET_ESP32H2) //ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = adc_unit,
                .bitwidth = __analogWidth,
                .atten = __analogAttenuation,
            };
            err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_handle[adc_unit].adc_cali_handle);
        #endif
        if(err != ESP_OK){
            log_e("adc_cali_create_scheme_x failed!");
            return value;
        }
    }

    err = adc_oneshot_get_calibrated_result(adc_handle[adc_unit].adc_oneshot_handle, adc_handle[adc_unit].adc_cali_handle, channel, &value);
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

/*
 *  ADC Continuous mode
 */

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    #define ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
    #define ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
    #define ADC_GET_DATA(p_data)                ((p_data)->type1.data)
#else
    #define ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
    #define ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
    #define ADC_GET_DATA(p_data)                ((p_data)->type2.data)
#endif

static uint8_t __adcContinuousAtten = ADC_11db;
static uint8_t __adcContinuousWidth = SOC_ADC_DIGI_MAX_BITWIDTH; 

static uint8_t used_adc_channels = 0;
adc_continuos_data_t * adc_result = NULL;

static bool adcContinuousDetachBus(void * adc_unit_number){
    adc_unit_t adc_unit = (adc_unit_t)adc_unit_number - 1;

    if(adc_handle[adc_unit].adc_continuous_handle == NULL){
        return true;
    }
    else
    {
        esp_err_t err = adc_continuous_deinit(adc_handle[adc_unit].adc_continuous_handle);
        if(err != ESP_OK){
            return false;
        }
        adc_handle[adc_unit].adc_continuous_handle = NULL;
        if(adc_handle[adc_unit].adc_cali_handle != NULL){
    #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED  
            err = adc_cali_delete_scheme_curve_fitting(adc_handle[adc_unit].adc_cali_handle);
            if(err != ESP_OK){
                return false;
            }
    #elif !defined(CONFIG_IDF_TARGET_ESP32H2)
            err = adc_cali_delete_scheme_line_fitting(adc_handle[adc_unit].adc_cali_handle);
            if(err != ESP_OK){
                return false;
            }
    #endif
        }
        adc_handle[adc_unit].adc_cali_handle = NULL;

        //set all used pins to INIT state
        for (uint8_t channel = 0; channel < SOC_ADC_CHANNEL_NUM(adc_unit); channel++){
            int io_pin;
            adc_oneshot_channel_to_io(adc_unit, channel, &io_pin);
            if(perimanGetPinBusType(io_pin) == ESP32_BUS_TYPE_ADC_CONT){
                if(!perimanClearPinBus(io_pin)){
                    return false;
                }
            }
        }
    }
    return true;
}

bool IRAM_ATTR adcFnWrapper(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *args){
    interrupt_config_t * isr = (interrupt_config_t*)args;
    //Check if edata->size matches conversion_frame_size, else just return from ISR
    if(edata->size == adc_handle[0].conversion_frame_size){
        if(isr->fn) {
            if(isr->arg){
                ((voidFuncPtrArg)isr->fn)(isr->arg);
            } else {
                isr->fn();
            }
        }
    }
    return false;
}

esp_err_t __analogContinuousInit(adc_channel_t *channel, uint8_t channel_num, adc_unit_t adc_unit, uint32_t sampling_freq_hz){
    //Create new ADC continuous handle
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = adc_handle[adc_unit].buffer_size,
        .conv_frame_size = adc_handle[adc_unit].conversion_frame_size,
    };

    esp_err_t err = adc_continuous_new_handle(&adc_config, &adc_handle[adc_unit].adc_continuous_handle);
    if(err != ESP_OK){
        log_e("adc_continuous_new_handle failed with error: %d", err);
        return ESP_FAIL;
    }

    //Configure adc pins
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = sampling_freq_hz,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_OUTPUT_TYPE,
    };
    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = __adcContinuousAtten;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = __adcContinuousWidth;
    }
    dig_cfg.adc_pattern = adc_pattern;
    err = adc_continuous_config(adc_handle[adc_unit].adc_continuous_handle, &dig_cfg);
    
    if(err != ESP_OK){
        log_e("adc_continuous_config failed with error: %d", err);
        return ESP_FAIL;
    }

    used_adc_channels = channel_num;
    return ESP_OK;
}


bool analogContinuous(uint8_t pins[], size_t pins_count, uint32_t conversions_per_pin, uint32_t sampling_freq_hz, void (*userFunc)(void)){
    adc_channel_t channel[pins_count];
    adc_unit_t adc_unit;
    esp_err_t err = ESP_OK;

    //Convert pins to channels and check if all are ADC1s unit
    for(int i = 0; i < pins_count; i++){
        err = adc_continuous_io_to_channel(pins[i], &adc_unit, &channel[i]);
        if(err != ESP_OK){
            log_e("Pin %u is not ADC pin!", pins[i]);
            return false;
        }
        if(adc_unit != 0){
            log_e("Only ADC1 pins are supported in continuous mode!");
            return false;
        }
    }

    //Check if Oneshot and Continous handle exists
    if(adc_handle[adc_unit].adc_oneshot_handle != NULL){
        log_e("ADC%d is running in oneshot mode. Aborting.", adc_unit+1);
        return false;
    }
    if(adc_handle[adc_unit].adc_continuous_handle != NULL){
        log_e("ADC%d continuous is already initialized. To reconfigure call analogContinuousDeinit() first.", adc_unit+1);
        return false;
    }

    //Check sampling frequency
    if((sampling_freq_hz < SOC_ADC_SAMPLE_FREQ_THRES_LOW) || (sampling_freq_hz > SOC_ADC_SAMPLE_FREQ_THRES_HIGH)){
        log_e("Sampling frequency is out of range. Supported sampling frequencies are %d - %d", SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH);
        return false;
    }

    //Set periman deinit function and reset all pins to init state.
    perimanSetBusDeinit(ESP32_BUS_TYPE_ADC_CONT, adcContinuousDetachBus);
    for(int j = 0; j < pins_count; j++){
        if(!perimanClearPinBus(pins[j])){
            return false;
        }
    }

    //Set conversion frame and buffer size (conversion frame must be in multiples of SOC_ADC_DIGI_DATA_BYTES_PER_CONV)
    adc_handle[adc_unit].conversion_frame_size = conversions_per_pin * pins_count * SOC_ADC_DIGI_RESULT_BYTES;

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    uint8_t calc_multiple = adc_handle[adc_unit].conversion_frame_size % SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
    if(calc_multiple != 0){
        adc_handle[adc_unit].conversion_frame_size = (adc_handle[adc_unit].conversion_frame_size + calc_multiple);
    }
#endif

    adc_handle[adc_unit].buffer_size = adc_handle[adc_unit].conversion_frame_size * 2;

    //Conversion frame size buffer cant be bigger than 4092 bytes
    if(adc_handle[adc_unit].conversion_frame_size > 4092){
        log_e("Buffers are too big. Please set lower conversions per pin.");
        return false;
    }

    //Initialize continuous handle and pins
    err = __analogContinuousInit(channel, sizeof(channel) / sizeof(adc_channel_t), adc_unit, sampling_freq_hz);
    if(err != ESP_OK){
        log_e("Analog initialization failed!");
        return false;
    }

    //Setup callbacks for complete event
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = adcFnWrapper,
        //.on_pool_ovf can be used in future
    };
    adc_handle[adc_unit].adc_interrupt_handle.fn = (voidFuncPtr)userFunc;
    err = adc_continuous_register_event_callbacks(adc_handle[adc_unit].adc_continuous_handle, &cbs, &adc_handle[adc_unit].adc_interrupt_handle);
    if(err != ESP_OK){
        log_e("adc_continuous_register_event_callbacks failed!");
        return false;
    }

    //Allocate and prepare result structure for adc readings
    adc_result = malloc(pins_count * sizeof(adc_continuos_data_t));
    for(int k = 0; k < pins_count; k++){
        adc_result[k].pin = pins[k];
        adc_result[k].channel = channel[k];
    }

    //Initialize ADC calibration handle
    if(adc_handle[adc_unit].adc_cali_handle == NULL){
        log_d("Creating cali handle for ADC_%d", adc_unit);
        #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = adc_unit,
                .atten = __adcContinuousAtten,
                .bitwidth = __adcContinuousWidth,
            };
            err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_handle[adc_unit].adc_cali_handle);
        #elif !defined(CONFIG_IDF_TARGET_ESP32H2) //ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = adc_unit,
                .bitwidth = __adcContinuousWidth,
                .atten = __adcContinuousAtten,
            };
            err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_handle[adc_unit].adc_cali_handle);
        #endif
        if(err != ESP_OK){
            log_e("adc_cali_create_scheme_x failed!");
            return false;
        }
    }

    for(int k = 0; k < pins_count; k++){
        if(!perimanSetPinBus(pins[k], ESP32_BUS_TYPE_ADC_CONT, (void *)(adc_unit+1), adc_unit, channel[k])){
            log_e("perimanSetPinBus to ADC Continuous failed!");
            adcContinuousDetachBus((void *)(adc_unit+1));
            return false;
        }
    }

    return true;
}

bool analogContinuousRead(adc_continuos_data_t ** buffer, uint32_t timeout_ms){
    if(adc_handle[ADC_UNIT_1].adc_continuous_handle != NULL){
        uint32_t bytes_read = 0;
        uint32_t read_raw[used_adc_channels];
        uint32_t read_count[used_adc_channels];
        uint8_t adc_read[adc_handle[ADC_UNIT_1].conversion_frame_size];
        memset(adc_read, 0xcc, sizeof(adc_read));
        memset(read_raw, 0, sizeof(read_raw));
        memset(read_count, 0, sizeof(read_count));

        esp_err_t err = adc_continuous_read(adc_handle[ADC_UNIT_1].adc_continuous_handle, adc_read, adc_handle[0].conversion_frame_size, &bytes_read, timeout_ms);
        if(err != ESP_OK){
            if(err == ESP_ERR_TIMEOUT){
                log_e("Reading data failed: No data, increase timeout");
            }
            else {
                log_e("Reading data failed with error: %X", err);
            }
            *buffer = NULL;
            return false;
        }

        for (int i = 0; i < bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t*)&adc_read[i];
            uint32_t chan_num = ADC_GET_CHANNEL(p);
            uint32_t data = ADC_GET_DATA(p);
            
            /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
            if(chan_num >= SOC_ADC_CHANNEL_NUM(0)){
                log_e("Invalid data [%d_%d]", chan_num, data);
                *buffer = NULL;
                return false;
            }
            if(data >= (1 << SOC_ADC_DIGI_MAX_BITWIDTH))
            {
                data = 0;
                log_e("Invalid data");
            }

            for(int j = 0; j < used_adc_channels; j++){
                if(adc_result[j].channel == chan_num){
                    read_raw[j] += data;
                    read_count[j] += 1;
                    break;
                }
            }
        }

        for (int j = 0; j < used_adc_channels; j++){
            if (read_count[j] != 0){
                adc_result[j].avg_read_raw = read_raw[j] / read_count[j];
                adc_cali_raw_to_voltage(adc_handle[ADC_UNIT_1].adc_cali_handle, adc_result[j].avg_read_raw, &adc_result[j].avg_read_mvolts);
            }
            else {
                log_w("No data read for pin %d", adc_result[j].pin);
            }
        }

        *buffer = adc_result;
        return true;

    }
    else {
        log_e("ADC Continuous is not initialized!");
        return false;
    }
}

bool analogContinuousStart(){
    if(adc_handle[ADC_UNIT_1].adc_continuous_handle != NULL){
        if(adc_continuous_start(adc_handle[ADC_UNIT_1].adc_continuous_handle) == ESP_OK){
            return true;
        }
    } else {
        log_e("ADC Continuous is not initialized!");
    }
    return false;
}

bool analogContinuousStop(){
    if(adc_handle[ADC_UNIT_1].adc_continuous_handle != NULL){
        if(adc_continuous_stop(adc_handle[ADC_UNIT_1].adc_continuous_handle) == ESP_OK){
            return true;
        }
    } else {
        log_e("ADC Continuous is not initialized!");
    }
    return false;
}

bool analogContinuousDeinit(){
    if(adc_handle[ADC_UNIT_1].adc_continuous_handle != NULL){
        esp_err_t err = adc_continuous_deinit(adc_handle[ADC_UNIT_1].adc_continuous_handle);
        if (err != ESP_OK){
            return false;
        }
        free(adc_result);
        adc_handle[ADC_UNIT_1].adc_continuous_handle = NULL;
    } else {
        log_i("ADC Continuous was not initialized");
    }
    return true;
}

void analogContinuousSetAtten(adc_attenuation_t attenuation){
    __adcContinuousAtten = attenuation;
}

void analogContinuousSetWidth(uint8_t bits){
    if ((bits < SOC_ADC_DIGI_MIN_BITWIDTH) && (bits > SOC_ADC_DIGI_MAX_BITWIDTH)){
        log_e("Selected width cannot be set. Range is from %d to %d", SOC_ADC_DIGI_MIN_BITWIDTH, SOC_ADC_DIGI_MAX_BITWIDTH);
        return;
    }
    __adcContinuousWidth = bits;
}

#endif
