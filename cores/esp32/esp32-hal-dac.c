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

#ifndef SOC_DAC_SUPPORTED           
#define NODAC
#else
#include "soc/dac_channel.h"
#include "driver/dac_common.h"

void ARDUINO_ISR_ATTR __dacWrite(uint8_t pin, uint8_t value)
{
    if(pin < DAC_CHANNEL_1_GPIO_NUM || pin > DAC_CHANNEL_2_GPIO_NUM){
        return;//not dac pin
    }

    uint8_t channel = pin - DAC_CHANNEL_1_GPIO_NUM;
    dac_output_enable(channel);
    dac_output_voltage(channel, value);

}

void ARDUINO_ISR_ATTR __dacDisable(uint8_t pin)
{
    if(pin < DAC_CHANNEL_1_GPIO_NUM || pin > DAC_CHANNEL_2_GPIO_NUM){
        return;//not dac pin
    }

    uint8_t channel = pin - DAC_CHANNEL_1_GPIO_NUM;
    dac_output_disable(channel);
}

extern void dacWrite(uint8_t pin, uint8_t value) __attribute__ ((weak, alias("__dacWrite")));
extern void dacDisable(uint8_t pin) __attribute__ ((weak, alias("__dacDisable")));

#endif
