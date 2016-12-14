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
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

static uint8_t __analogAttenuation = 3;//11db
static uint8_t __analogWidth = 3;//12 bits
static uint8_t __analogCycles = 8;
static uint8_t __analogSamples = 0;//1 sample
static uint8_t __analogClockDiv = 1;

void __analogSetWidth(uint8_t bits){
    if(bits < 9){
        bits = 9;
    } else if(bits > 12){
        bits = 12;
    }
    __analogWidth = bits - 9;
    SET_PERI_REG_BITS(SENS_SAR_START_FORCE_REG, SENS_SAR1_BIT_WIDTH, __analogWidth, SENS_SAR1_BIT_WIDTH_S);
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL_REG, SENS_SAR1_SAMPLE_BIT, __analogWidth, SENS_SAR1_SAMPLE_BIT_S);

    SET_PERI_REG_BITS(SENS_SAR_START_FORCE_REG, SENS_SAR2_BIT_WIDTH, __analogWidth, SENS_SAR2_BIT_WIDTH_S);
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL2_REG, SENS_SAR2_SAMPLE_BIT, __analogWidth, SENS_SAR2_SAMPLE_BIT_S);
}

void __analogSetCycles(uint8_t cycles){
    __analogCycles = cycles;
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL_REG, SENS_SAR1_SAMPLE_CYCLE, __analogCycles, SENS_SAR1_SAMPLE_CYCLE_S);
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL2_REG, SENS_SAR2_SAMPLE_CYCLE, __analogCycles, SENS_SAR2_SAMPLE_CYCLE_S);
}

void __analogSetSamples(uint8_t samples){
    if(!samples){
        return;
    }
    __analogSamples = samples - 1;
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL_REG, SENS_SAR1_SAMPLE_NUM, __analogSamples, SENS_SAR1_SAMPLE_NUM_S);
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL2_REG, SENS_SAR2_SAMPLE_NUM, __analogSamples, SENS_SAR2_SAMPLE_NUM_S);
}

void __analogSetClockDiv(uint8_t clockDiv){
    if(!clockDiv){
        return;
    }
    __analogClockDiv = clockDiv;
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL_REG, SENS_SAR1_CLK_DIV, __analogClockDiv, SENS_SAR1_CLK_DIV_S);
    SET_PERI_REG_BITS(SENS_SAR_READ_CTRL2_REG, SENS_SAR2_CLK_DIV, __analogClockDiv, SENS_SAR2_CLK_DIV_S);
}

void __analogSetAttenuation(adc_attenuation_t attenuation)
{
    __analogAttenuation = attenuation & 3;
    uint32_t att_data = 0;
    int i = 10;
    while(i--){
        att_data |= __analogAttenuation << (i * 2);
    }
    WRITE_PERI_REG(SENS_SAR_ATTEN1_REG, att_data & 0xFFFF);//ADC1 has 8 channels
    WRITE_PERI_REG(SENS_SAR_ATTEN2_REG, att_data);
}

void IRAM_ATTR __analogInit(){
    static bool initialized = false;
    if(initialized){
        return;
    }

    __analogSetAttenuation(__analogAttenuation);
    __analogSetCycles(__analogCycles);
    __analogSetSamples(__analogSamples + 1);//in samples
    __analogSetClockDiv(__analogClockDiv);
    __analogSetWidth(__analogWidth + 9);//in bits

    SET_PERI_REG_MASK(SENS_SAR_READ_CTRL_REG, SENS_SAR1_DATA_INV);
    SET_PERI_REG_MASK(SENS_SAR_READ_CTRL2_REG, SENS_SAR2_DATA_INV);

    SET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_START_FORCE_M); //SAR ADC1 controller (in RTC) is started by SW
    SET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_SAR1_EN_PAD_FORCE_M); //SAR ADC1 pad enable bitmap is controlled by SW
    SET_PERI_REG_MASK(SENS_SAR_MEAS_START2_REG, SENS_MEAS2_START_FORCE_M); //SAR ADC2 controller (in RTC) is started by SW
    SET_PERI_REG_MASK(SENS_SAR_MEAS_START2_REG, SENS_SAR2_EN_PAD_FORCE_M); //SAR ADC2 pad enable bitmap is controlled by SW

    CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR_M); //force XPD_SAR=0, use XPD_FSM
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_AMP, 0x2, SENS_FORCE_XPD_AMP_S); //force XPD_AMP=0

    CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_CTRL_REG, 0xfff << SENS_AMP_RST_FB_FSM_S);  //clear FSM
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT1_REG, SENS_SAR_AMP_WAIT1, 0x1, SENS_SAR_AMP_WAIT1_S);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT1_REG, SENS_SAR_AMP_WAIT2, 0x1, SENS_SAR_AMP_WAIT2_S);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_SAR_AMP_WAIT3, 0x1, SENS_SAR_AMP_WAIT3_S);
    while (GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR1_REG, 0x7, SENS_MEAS_STATUS_S) != 0); //wait det_fsm==

    initialized = true;
}

void __analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation)
{
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0 || attenuation > 3){
        return ;
    }
    __analogInit();
    if(channel > 7){
        SET_PERI_REG_BITS(SENS_SAR_ATTEN2_REG, 3, attenuation, ((channel - 10) * 2));
    } else {
        SET_PERI_REG_BITS(SENS_SAR_ATTEN1_REG, 3, attenuation, (channel * 2));
    }
}

uint16_t IRAM_ATTR __analogRead(uint8_t pin)
{
    int8_t channel = digitalPinToAnalogChannel(pin);
    if(channel < 0){
        return 0;//not adc pin
    }
    int8_t pad = digitalPinToTouchChannel(pin);

    if(pad >= 0){
        uint32_t touch = READ_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG);
        if(touch & (1 << pad)){
            touch &= ~((1 << (pad + SENS_TOUCH_PAD_OUTEN2_S))
                    | (1 << (pad + SENS_TOUCH_PAD_OUTEN1_S))
                    | (1 << (pad + SENS_TOUCH_PAD_WORKEN_S)));
            WRITE_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG, touch);
        }
    } else if(pin == 25){
        CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC | RTC_IO_PDAC1_DAC_XPD_FORCE);//stop dac1
    } else if(pin == 26){
        CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_XPD_DAC | RTC_IO_PDAC2_DAC_XPD_FORCE);//stop dac2
    }

    pinMode(pin, ANALOG);

    __analogInit();

    if(channel > 7){
        channel -= 10;

        SET_PERI_REG_BITS(SENS_SAR_MEAS_START2_REG, SENS_SAR2_EN_PAD, (1 << channel), SENS_SAR2_EN_PAD_S);
        CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START2_REG, SENS_MEAS2_START_SAR_M);
        SET_PERI_REG_MASK(SENS_SAR_MEAS_START2_REG, SENS_MEAS2_START_SAR_M);
        while (GET_PERI_REG_MASK(SENS_SAR_MEAS_START2_REG, SENS_MEAS2_DONE_SAR) == 0) {}; //read done
        return GET_PERI_REG_BITS2(SENS_SAR_MEAS_START2_REG, SENS_MEAS2_DATA_SAR, SENS_MEAS2_DATA_SAR_S);
    }

    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, SENS_SAR1_EN_PAD, (1 << channel), SENS_SAR1_EN_PAD_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_START_SAR_M);
    SET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_START_SAR_M);
    while (GET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_DONE_SAR) == 0) {}; //read done
    return GET_PERI_REG_BITS2(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_DATA_SAR, SENS_MEAS1_DATA_SAR_S);
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

extern uint16_t analogRead(uint8_t pin) __attribute__ ((weak, alias("__analogRead")));
extern void analogSetWidth(uint8_t bits) __attribute__ ((weak, alias("__analogSetWidth")));
extern void analogSetCycles(uint8_t cycles) __attribute__ ((weak, alias("__analogSetCycles")));
extern void analogSetSamples(uint8_t samples) __attribute__ ((weak, alias("__analogSetSamples")));
extern void analogSetClockDiv(uint8_t clockDiv) __attribute__ ((weak, alias("__analogSetClockDiv")));
extern void analogSetAttenuation(adc_attenuation_t attenuation) __attribute__ ((weak, alias("__analogSetAttenuation")));
extern void analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation) __attribute__ ((weak, alias("__analogSetPinAttenuation")));
extern int hallRead() __attribute__ ((weak, alias("__hallRead")));
