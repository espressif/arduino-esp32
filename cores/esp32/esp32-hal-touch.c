// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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

#include "esp32-hal-touch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

static uint16_t __touchSleepCycles = 0x1000;
static uint16_t __touchMeasureCycles = 0x1000;

typedef void (*voidFuncPtr)(void);
static voidFuncPtr __touchInterruptHandlers[10] = {0,};
static intr_handle_t touch_intr_handle = NULL;

void IRAM_ATTR __touchISR(void * arg)
{
    uint32_t pad_intr = READ_PERI_REG(SENS_SAR_TOUCH_CTRL2_REG) & 0x3ff;
    uint32_t rtc_intr = READ_PERI_REG(RTC_CNTL_INT_ST_REG);
    uint8_t i = 0;
    //clear interrupt
    WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, rtc_intr);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);

    if (rtc_intr & RTC_CNTL_TOUCH_INT_ST) {
        for (i = 0; i < 10; ++i) {
            if ((pad_intr >> i) & 0x01) {
                if(__touchInterruptHandlers[i]){
                    __touchInterruptHandlers[i]();
                }
            }
        }
    }
}

void __touchSetCycles(uint16_t measure, uint16_t sleep)
{
    __touchSleepCycles = sleep;
    __touchMeasureCycles = measure;
    //Touch pad SleepCycle Time
    SET_PERI_REG_BITS(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_SLEEP_CYCLES, __touchSleepCycles, SENS_TOUCH_SLEEP_CYCLES_S);
    //Touch Pad Measure Time
    SET_PERI_REG_BITS(SENS_SAR_TOUCH_CTRL1_REG, SENS_TOUCH_MEAS_DELAY, __touchMeasureCycles, SENS_TOUCH_MEAS_DELAY_S);
}

void __touchInit()
{
    static bool initialized = false;
    if(initialized){
        return;
    }
    initialized = true;
    SET_PERI_REG_BITS(RTC_IO_TOUCH_CFG_REG, RTC_IO_TOUCH_XPD_BIAS, 1, RTC_IO_TOUCH_XPD_BIAS_S);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);
    //clear touch enable
    WRITE_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG, 0x0);
    SET_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_TOUCH_SLP_TIMER_EN);

    __touchSetCycles(__touchMeasureCycles, __touchSleepCycles);

    esp_intr_alloc(ETS_RTC_CORE_INTR_SOURCE, (int)ESP_INTR_FLAG_IRAM, __touchISR, NULL, &touch_intr_handle);
}

uint16_t __touchRead(uint8_t pin)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        return 0;
    }

    pinMode(pin, ANALOG);

    __touchInit();

    uint32_t v0 = READ_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG);
    //Disable Intr & enable touch pad
    WRITE_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG,
            (v0 & ~((1 << (pad + SENS_TOUCH_PAD_OUTEN2_S)) | (1 << (pad + SENS_TOUCH_PAD_OUTEN1_S))))
            | (1 << (pad + SENS_TOUCH_PAD_WORKEN_S)));

    SET_PERI_REG_MASK(SENS_SAR_TOUCH_ENABLE_REG, (1 << (pad + SENS_TOUCH_PAD_WORKEN_S)));

    uint32_t rtc_tio_reg = RTC_IO_TOUCH_PAD0_REG + pad * 4;
    WRITE_PERI_REG(rtc_tio_reg, (READ_PERI_REG(rtc_tio_reg)
                      & ~(RTC_IO_TOUCH_PAD0_DAC_M))
                      | (7 << RTC_IO_TOUCH_PAD0_DAC_S)//Touch Set Slope
                      | RTC_IO_TOUCH_PAD0_TIE_OPT_M   //Enable Tie,Init Level
                      | RTC_IO_TOUCH_PAD0_START_M     //Enable Touch Pad IO
                      | RTC_IO_TOUCH_PAD0_XPD_M);     //Enable Touch Pad Power on

    //force oneTime test start
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_START_EN_M|SENS_TOUCH_START_FORCE_M);

    SET_PERI_REG_BITS(SENS_SAR_TOUCH_CTRL1_REG, SENS_TOUCH_XPD_WAIT, 10, SENS_TOUCH_XPD_WAIT_S);

    while (GET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_DONE) == 0) {};

    uint16_t touch_value = READ_PERI_REG(SENS_SAR_TOUCH_OUT1_REG + (pad / 2) * 4) >> ((pad & 1) ? SENS_TOUCH_MEAS_OUT1_S : SENS_TOUCH_MEAS_OUT0_S);

    //clear touch force ,select the Touch mode is Timer
    CLEAR_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_START_EN_M|SENS_TOUCH_START_FORCE_M);

    //restore previous value
    WRITE_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG, v0);
    return touch_value;
}

void __touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), uint16_t threshold)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        return;
    }

    pinMode(pin, ANALOG);

    __touchInit();

    __touchInterruptHandlers[pad] = userFunc;

    //clear touch force ,select the Touch mode is Timer
    CLEAR_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_START_EN_M|SENS_TOUCH_START_FORCE_M);

    //interrupt when touch value < threshold
    CLEAR_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL1_REG, SENS_TOUCH_OUT_SEL);
    //Intr will give ,when SET0 < threshold
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL1_REG, SENS_TOUCH_OUT_1EN);
    //Enable Rtc Touch Module Intr,the Interrupt need Rtc out  Enable
    SET_PERI_REG_MASK(RTC_CNTL_INT_ENA_REG, RTC_CNTL_TOUCH_INT_ENA);

    //set threshold
    uint8_t shift = (pad & 1) ? SENS_TOUCH_OUT_TH1_S : SENS_TOUCH_OUT_TH0_S;
    SET_PERI_REG_BITS((SENS_SAR_TOUCH_THRES1_REG + (pad / 2) * 4), SENS_TOUCH_OUT_TH0, threshold, shift);

    uint32_t rtc_tio_reg = RTC_IO_TOUCH_PAD0_REG + pad * 4;
    WRITE_PERI_REG(rtc_tio_reg, (READ_PERI_REG(rtc_tio_reg)
                      & ~(RTC_IO_TOUCH_PAD0_DAC_M))
                      | (7 << RTC_IO_TOUCH_PAD0_DAC_S)//Touch Set Slope
                      | RTC_IO_TOUCH_PAD0_TIE_OPT_M   //Enable Tie,Init Level
                      | RTC_IO_TOUCH_PAD0_START_M     //Enable Touch Pad IO
                      | RTC_IO_TOUCH_PAD0_XPD_M);     //Enable Touch Pad Power on

    //Enable Digital rtc control :work mode and out mode
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_ENABLE_REG,
                      (1 << (pad + SENS_TOUCH_PAD_WORKEN_S)) | \
                      (1 << (pad + SENS_TOUCH_PAD_OUTEN2_S)) | \
                      (1 << (pad + SENS_TOUCH_PAD_OUTEN1_S)));
}

extern uint16_t touchRead(uint8_t pin) __attribute__ ((weak, alias("__touchRead")));
extern void touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), uint16_t threshold) __attribute__ ((weak, alias("__touchAttachInterrupt")));
extern void touchSetCycles(uint16_t measure, uint16_t sleep) __attribute__ ((weak, alias("__touchSetCycles")));
