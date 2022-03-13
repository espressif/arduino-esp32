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



#include "esp32-hal.h"
#include "soc/soc_caps.h"
#include "driver/sigmadelta.h"

static uint8_t duty_set[SOC_SIGMADELTA_CHANNEL_NUM] = {0};
static uint32_t prescaler_set[SOC_SIGMADELTA_CHANNEL_NUM] = {0};

static void _on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb){
    if(old_apb == new_apb){
        return;
    }
    uint32_t iarg = (uint32_t)arg;
    uint8_t channel = iarg;
    if(ev_type == APB_AFTER_CHANGE){
        old_apb /= 1000000;
        new_apb /= 1000000;
        uint32_t old_prescale = prescaler_set[channel] + 1;
        uint32_t new_prescale = ((new_apb * old_prescale) / old_apb) - 1;
        sigmadelta_set_prescale(channel,new_prescale);
        prescaler_set[channel] = new_prescale;
    }
}

uint32_t sigmaDeltaSetup(uint8_t pin, uint8_t channel, uint32_t freq) //chan 0-x according to SOC, freq 1220-312500
{
    if(channel >= SOC_SIGMADELTA_CHANNEL_NUM){
        return 0;
    }
    
    uint32_t apb_freq = getApbFrequency();
    uint32_t prescale = (apb_freq/(freq*256)) - 1;
    if(prescale > 0xFF) {
        prescale = 0xFF;
    }

    sigmadelta_config_t sigmadelta_cfg = {
        .channel = channel,
        .sigmadelta_prescale = prescale,
        .sigmadelta_duty = 0,
        .sigmadelta_gpio = pin,
    };
    sigmadelta_config(&sigmadelta_cfg);

    prescaler_set[channel] = prescale;
    uint32_t iarg = channel;
    addApbChangeCallback((void*)iarg, _on_apb_change);

    return apb_freq/((prescale + 1) * 256);
}

void sigmaDeltaWrite(uint8_t channel, uint8_t duty) //chan 0-x according to SOC duty 8 bit
{
    if(channel >= SOC_SIGMADELTA_CHANNEL_NUM){
        return;
    }
    duty -= 128; 

    sigmadelta_set_duty(channel,duty);
    duty_set[channel] = duty;
}

uint8_t sigmaDeltaRead(uint8_t channel) //chan 0-x according to SOC
{
    if(channel >= SOC_SIGMADELTA_CHANNEL_NUM){
        return 0;
    }
    return duty_set[channel]+128;
}

void sigmaDeltaDetachPin(uint8_t pin)
{
    pinMatrixOutDetach(pin, false, false);
}