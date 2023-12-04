/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32-hal-dac.h"

#if SOC_DAC_SUPPORTED
#include "esp32-hal.h"
#include "esp32-hal-periman.h"
#include "soc/dac_channel.h"
#include "driver/dac_oneshot.h"

static bool dacDetachBus(void * bus){
    esp_err_t err = dac_oneshot_del_channel((dac_oneshot_handle_t)bus);
    if(err != ESP_OK){
        log_e("dac_oneshot_del_channel failed with error: %d", err);
        return false;
    }
    return true;
}

bool __dacWrite(uint8_t pin, uint8_t value)
{
    esp_err_t err = ESP_OK;
    if(pin != DAC_CHAN0_GPIO_NUM && pin != DAC_CHAN1_GPIO_NUM){
        log_e("pin %u is not a DAC pin", pin);
        return false;//not dac pin
    }

    dac_oneshot_handle_t bus = (dac_oneshot_handle_t)perimanGetPinBus(pin, ESP32_BUS_TYPE_DAC_ONESHOT);
    if(bus == NULL){
        perimanSetBusDeinit(ESP32_BUS_TYPE_DAC_ONESHOT, dacDetachBus);
        if(!perimanClearPinBus(pin)){
             return false;
        }
        dac_channel_t channel = (pin == DAC_CHAN0_GPIO_NUM)?DAC_CHAN_0:DAC_CHAN_1;
        dac_oneshot_config_t config = {
            .chan_id = channel
        };
        err = dac_oneshot_new_channel(&config, &bus);
        if(err != ESP_OK){
            log_e("dac_oneshot_new_channel failed with error: %d", err);
            return false;
        }
        if(!perimanSetPinBus(pin, ESP32_BUS_TYPE_DAC_ONESHOT, (void *)bus, -1, channel)){
            dacDetachBus((void *)bus);
            return false;
        }
    }

    err = dac_oneshot_output_voltage(bus, value);
    if(err != ESP_OK){
        log_e("dac_oneshot_output_voltage failed with error: %d", err);
        return false;
    }
    return true;
}

bool __dacDisable(uint8_t pin)
{
    if(pin != DAC_CHAN0_GPIO_NUM && pin != DAC_CHAN1_GPIO_NUM){
        log_e("pin %u is not a DAC pin", pin);
        return false;//not dac pin
    }
    void * bus = perimanGetPinBus(pin, ESP32_BUS_TYPE_DAC_ONESHOT);
    if(bus != NULL){
        // will call dacDetachBus
        return perimanClearPinBus(pin);
    } else {
        log_e("pin %u is not attached to DAC", pin);
    }
    return false;
}

extern bool dacWrite(uint8_t pin, uint8_t value) __attribute__ ((weak, alias("__dacWrite")));
extern bool dacDisable(uint8_t pin) __attribute__ ((weak, alias("__dacDisable")));

#endif
