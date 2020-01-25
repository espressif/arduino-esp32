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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

#include "driver/adc.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp_adc_cal.h"
#include "esp32/rom/ets_sys.h"
#include "esp_intr_alloc.h"
#define DEFAULT_VREF    1100
static esp_adc_cal_characteristics_t *__analogCharacteristics[2] = {NULL, NULL};
static uint16_t __analogVRef = 0;
static uint8_t __analogVRefPin = 0;
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/ets_sys.h"
#include "esp_intr.h"
#endif

static uint8_t __analogAttenuation = 3;//11db
static uint8_t __analogWidth = 3;//12 bits
static uint8_t __analogClockDiv = 1;

void __analogSetClockDiv(uint8_t clockDiv){
    if(!clockDiv){
        clockDiv = 1;
    }
    __analogClockDiv = clockDiv;
    adc_set_clk_div(__analogClockDiv);
}

void __analogSetAttenuation(adc_attenuation_t attenuation)
{
    __analogAttenuation = attenuation & 3;
}

#if CONFIG_IDF_TARGET_ESP32
void __analogSetWidth(uint8_t bits){
    if(bits < 9){
        bits = 9;
    } else if(bits > 12){
        bits = 12;
    }
    __analogWidth = bits - 9;
    adc1_config_width(__analogWidth);
}
#endif

void __analogInit(){
    static bool initialized = false;
    if(initialized){
        return;
    }
    initialized = true;
    __analogSetClockDiv(__analogClockDiv);
#if CONFIG_IDF_TARGET_ESP32
    __analogSetWidth(__analogWidth + 9);//in bits
#endif
}

void __analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation)
{
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0 || attenuation > 3){
        return ;
    }
    if(channel > 9){
        adc2_config_channel_atten(channel - 10, attenuation);
    } else {
        adc1_config_channel_atten(channel, attenuation);
    }
    __analogInit();
}

bool __adcAttachPin(uint8_t pin){
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0){
        log_e("Pin %u is not ADC pin!", pin);
        return false;
    }
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad >= 0){
#if CONFIG_IDF_TARGET_ESP32
        uint32_t touch = READ_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG);
        if(touch & (1 << pad)){
            touch &= ~((1 << (pad + SENS_TOUCH_PAD_OUTEN2_S))
                    | (1 << (pad + SENS_TOUCH_PAD_OUTEN1_S))
                    | (1 << (pad + SENS_TOUCH_PAD_WORKEN_S)));
            WRITE_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG, touch);
        }
#endif
    } else if(pin == 25){
        CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC | RTC_IO_PDAC1_DAC_XPD_FORCE);//stop dac1
    } else if(pin == 26){
        CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_XPD_DAC | RTC_IO_PDAC2_DAC_XPD_FORCE);//stop dac2
    }

    pinMode(pin, ANALOG);
    __analogSetPinAttenuation(pin, __analogAttenuation);
    return true;
}

void __analogReadResolution(uint8_t bits)
{
    if(!bits || bits > 16){
        return;
    }
#if CONFIG_IDF_TARGET_ESP32
    __analogSetWidth(bits);         // hadware from 9 to 12
#endif
}

uint16_t __analogRead(uint8_t pin)
{
    int8_t channel = digitalPinToAnalogChannel(pin);
    int value = 0;
    esp_err_t r = ESP_OK;
    if(channel < 0){
        log_e("Pin %u is not ADC pin!", pin);
        return value;
    }
    __adcAttachPin(pin);
    if(channel > 9){
        channel -= 10;
        r = adc2_get_raw( channel, __analogWidth, &value);
        if ( r == ESP_OK ) {
            return value;
        } else if ( r == ESP_ERR_INVALID_STATE ) {
            log_e("GPIO%u: %s: ADC2 not initialized yet.", pin, esp_err_to_name(r));
        } else if ( r == ESP_ERR_TIMEOUT ) {
            log_e("GPIO%u: %s: ADC2 is in use by Wi-Fi.", pin, esp_err_to_name(r));
        } else {
            log_e("GPIO%u: %s", pin, esp_err_to_name(r));
        }
    } else {
        return adc1_get_raw(channel);
    }
    return value;
}

uint32_t __analogReadMilliVolts(uint8_t pin){
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0){
        log_e("Pin %u is not ADC pin!", pin);
        return 0;
    }
#if CONFIG_IDF_TARGET_ESP32
    if(!__analogVRef){
        if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
            log_d("eFuse Two Point: Supported");
            __analogVRef = DEFAULT_VREF;
        }
        if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
            log_d("eFuse Vref: Supported");
            __analogVRef = DEFAULT_VREF;
        }
        if(!__analogVRef){
            __analogVRef = DEFAULT_VREF;
            if(__analogVRefPin){
                esp_adc_cal_characteristics_t chars;
                if(adc2_vref_to_gpio(__analogVRefPin) == ESP_OK){
                    __analogVRef = __analogRead(__analogVRefPin);
                    esp_adc_cal_characterize(1, __analogAttenuation, __analogWidth, DEFAULT_VREF, &chars);
                    __analogVRef = esp_adc_cal_raw_to_voltage(__analogVRef, &chars);
                    log_d("Vref to GPIO%u: %u", __analogVRefPin, __analogVRef);
                }
            }
        }
    }
    uint8_t unit = 1;
    if(channel > 9){
        unit = 2;
    }
    uint16_t adc_reading = __analogRead(pin);
    if(__analogCharacteristics[unit - 1] == NULL){
        __analogCharacteristics[unit - 1] = calloc(1, sizeof(esp_adc_cal_characteristics_t));
        if(__analogCharacteristics[unit - 1] == NULL){
            return 0;
        }
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, __analogAttenuation, __analogWidth, __analogVRef, __analogCharacteristics[unit - 1]);
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            log_i("ADC%u: Characterized using Two Point Value: %u\n", unit, __analogCharacteristics[unit - 1]->vref);
        } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            log_i("ADC%u: Characterized using eFuse Vref: %u\n", unit, __analogCharacteristics[unit - 1]->vref);
        } else if(__analogVRef != DEFAULT_VREF){
            log_i("ADC%u: Characterized using Vref to GPIO%u: %u\n", unit, __analogVRefPin, __analogCharacteristics[unit - 1]->vref);
        } else {
            log_i("ADC%u: Characterized using Default Vref: %u\n", unit, __analogCharacteristics[unit - 1]->vref);
        }
    }
    return esp_adc_cal_raw_to_voltage(adc_reading, __analogCharacteristics[unit - 1]);
#else
    uint16_t adc_reading = __analogRead(pin);
    uint16_t max_reading = 8191;
    uint16_t max_mv = 1100;
    switch(__analogAttenuation){
        case 3: max_mv = 3900; break;
        case 2: max_mv = 2200; break;
        case 1: max_mv = 1500; break;
        default: break;
    }
    return (adc_reading * max_mv) / max_reading;
#endif
}

#if CONFIG_IDF_TARGET_ESP32

void __analogSetVRefPin(uint8_t pin){
    if(pin <25 || pin > 27){
        pin = 0;
    }
    __analogVRefPin = pin;
}

int __hallRead()    //hall sensor without LNA
{
    int Sens_Vp0;
    int Sens_Vn0;
    int Sens_Vp1;
    int Sens_Vn1;

    pinMode(36, ANALOG);
    pinMode(39, ANALOG);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL1_REG, SENS_XPD_HALL_FORCE_M);     // hall sens force enable
    SET_PERI_REG_MASK(RTC_IO_HALL_SENS_REG, RTC_IO_XPD_HALL);               // xpd hall
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL1_REG, SENS_HALL_PHASE_FORCE_M);   // phase force
    CLEAR_PERI_REG_MASK(RTC_IO_HALL_SENS_REG, RTC_IO_HALL_PHASE);           // hall phase
    Sens_Vp0 = __analogRead(36);
    Sens_Vn0 = __analogRead(39);
    SET_PERI_REG_MASK(RTC_IO_HALL_SENS_REG, RTC_IO_HALL_PHASE);
    Sens_Vp1 = __analogRead(36);
    Sens_Vn1 = __analogRead(39);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 0, SENS_FORCE_XPD_SAR_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL1_REG, SENS_XPD_HALL_FORCE);
    CLEAR_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL1_REG, SENS_HALL_PHASE_FORCE);
    return (Sens_Vp1 - Sens_Vp0) - (Sens_Vn1 - Sens_Vn0);
}
#endif

extern uint16_t analogRead(uint8_t pin) __attribute__ ((weak, alias("__analogRead")));
extern uint32_t analogReadMilliVolts(uint8_t pin) __attribute__ ((weak, alias("__analogReadMilliVolts")));
extern void analogReadResolution(uint8_t bits) __attribute__ ((weak, alias("__analogReadResolution")));
extern void analogSetClockDiv(uint8_t clockDiv) __attribute__ ((weak, alias("__analogSetClockDiv")));
extern void analogSetAttenuation(adc_attenuation_t attenuation) __attribute__ ((weak, alias("__analogSetAttenuation")));
extern void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation) __attribute__ ((weak, alias("__analogSetPinAttenuation")));

extern bool adcAttachPin(uint8_t pin) __attribute__ ((weak, alias("__adcAttachPin")));

#if CONFIG_IDF_TARGET_ESP32
extern void analogSetVRefPin(uint8_t pin) __attribute__ ((weak, alias("__analogSetVRefPin")));
extern void analogSetWidth(uint8_t bits) __attribute__ ((weak, alias("__analogSetWidth")));
extern int hallRead() __attribute__ ((weak, alias("__hallRead")));
#endif

