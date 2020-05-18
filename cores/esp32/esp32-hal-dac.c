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
#include "esp_attr.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_periph.h"
#include "soc/sens_reg.h"
#include "soc/sens_struct.h"
#include "driver/dac.h"

#if CONFIG_IDF_TARGET_ESP32
#define DAC1 25
#define DAC2 26
#elif CONFIG_IDF_TARGET_ESP32S2
#define DAC1 17
#define DAC2 18
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif

void ARDUINO_ISR_ATTR __dacWrite(uint8_t pin, uint8_t value)
{
    if(pin < DAC1 || pin > DAC2){
        return;//not dac pin
    }
    pinMode(pin, ANALOG);
    uint8_t channel = pin - DAC1;
#if CONFIG_IDF_TARGET_ESP32
    CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);
#elif CONFIG_IDF_TARGET_ESP32S2
    SENS.sar_dac_ctrl1.dac_clkgate_en = 1;
#endif
    RTCIO.pad_dac[channel].dac_xpd_force = 1;
    RTCIO.pad_dac[channel].xpd_dac = 1;
    if (channel == 0) {
        SENS.sar_dac_ctrl2.dac_cw_en1 = 0;
    } else if (channel == 1) {
        SENS.sar_dac_ctrl2.dac_cw_en2 = 0;
    }
    RTCIO.pad_dac[channel].dac = value;
}

extern void dacWrite(uint8_t pin, uint8_t value) __attribute__ ((weak, alias("__dacWrite")));
