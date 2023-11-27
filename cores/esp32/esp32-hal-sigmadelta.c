/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32-hal-sigmadelta.h"

#if SOC_SDM_SUPPORTED
#include "esp32-hal.h"
#include "esp32-hal-periman.h"
#include "driver/sdm.h"

static bool sigmaDeltaDetachBus(void * bus){
    esp_err_t err = sdm_channel_disable((sdm_channel_handle_t)bus);
    if(err != ESP_OK){
        log_w("sdm_channel_disable failed with error: %d", err);
    }
    err = sdm_del_channel((sdm_channel_handle_t)bus);
    if(err != ESP_OK){
        log_e("sdm_del_channel failed with error: %d", err);
        return false;
    }
    return true;
}

bool sigmaDeltaAttach(uint8_t pin, uint32_t freq) //freq 1220-312500
{
    perimanSetBusDeinit(ESP32_BUS_TYPE_SIGMADELTA, sigmaDeltaDetachBus);
    sdm_channel_handle_t bus = (sdm_channel_handle_t)perimanGetPinBus(pin, ESP32_BUS_TYPE_SIGMADELTA);
    if(bus != NULL && !perimanClearPinBus(pin)){
        return false;
    }
    bus = NULL;
    sdm_config_t config = {
        .gpio_num = (int)pin,
        .clk_src = SDM_CLK_SRC_DEFAULT,
        .sample_rate_hz = freq,
        .flags = {
            .invert_out = 0,
            .io_loop_back = 0
        }
    };
    esp_err_t err = sdm_new_channel(&config, &bus);
    if(err != ESP_OK){
        log_e("sdm_new_channel failed with error: %d", err);
        return false;
    }
    err = sdm_channel_enable(bus);
    if(err != ESP_OK){
        sigmaDeltaDetachBus((void *)bus);
        log_e("sdm_channel_enable failed with error: %d", err);
        return false;
    }
    if(!perimanSetPinBus(pin, ESP32_BUS_TYPE_SIGMADELTA, (void *)bus, -1, -1)){
        sigmaDeltaDetachBus((void *)bus);
        return false;
    }
    return true;
}

bool sigmaDeltaWrite(uint8_t pin, uint8_t duty) //chan 0-x according to SOC duty 8 bit
{
    sdm_channel_handle_t bus = (sdm_channel_handle_t)perimanGetPinBus(pin, ESP32_BUS_TYPE_SIGMADELTA);
    if(bus != NULL){
        int8_t d = duty - 128;
        esp_err_t err = sdm_channel_set_duty(bus, d);
        if(err != ESP_OK){
            log_e("sdm_channel_set_duty failed with error: %d", err);
            return false;
        }
        return true;
    } else {
        log_e("pin %u is not attached to SigmaDelta", pin);
    }
    return false;
}

bool sigmaDeltaDetach(uint8_t pin)
{
    void * bus = perimanGetPinBus(pin, ESP32_BUS_TYPE_SIGMADELTA);
    if(bus != NULL){
        // will call sigmaDeltaDetachBus
        return perimanClearPinBus(pin);
    } else {
        log_e("pin %u is not attached to SigmaDelta", pin);
    }
    return false;
}
#endif
