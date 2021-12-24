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

#include "esp32-hal-touch.h"
#ifndef CONFIG_IDF_TARGET_ESP32C3
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/sens_struct.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/touch_sensor.h"

#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/rom/ets_sys.h"
#include "esp_intr_alloc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif
#else // ESP32 Before IDF 4.0
#include "rom/ets_sys.h"
#include "esp_intr.h"
#endif

static uint16_t __touchSleepCycles = 0x1000;
static uint16_t __touchMeasureCycles = 0x1000;

typedef void (*voidFuncPtr)(void);
static voidFuncPtr __touchInterruptHandlers[10] = {0,};
static intr_handle_t touch_intr_handle = NULL;

void ARDUINO_ISR_ATTR __touchISR(void * arg)
{
#if CONFIG_IDF_TARGET_ESP32
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
#endif
}

void __touchSetCycles(uint16_t measure, uint16_t sleep)
{
    __touchSleepCycles = sleep;
    __touchMeasureCycles = measure;
#if CONFIG_IDF_TARGET_ESP32
    //Touch pad SleepCycle Time
    SET_PERI_REG_BITS(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_SLEEP_CYCLES, __touchSleepCycles, SENS_TOUCH_SLEEP_CYCLES_S);
    //Touch Pad Measure Time
    SET_PERI_REG_BITS(SENS_SAR_TOUCH_CTRL1_REG, SENS_TOUCH_MEAS_DELAY, __touchMeasureCycles, SENS_TOUCH_MEAS_DELAY_S);
#else
    touch_pad_set_meas_time(sleep, measure);
#endif
}

void __touchInit()
{
    static bool initialized = false;
    if(initialized){
        return;
    }
    initialized = true;
#if CONFIG_IDF_TARGET_ESP32
    SET_PERI_REG_BITS(RTC_IO_TOUCH_CFG_REG, RTC_IO_TOUCH_XPD_BIAS, 1, RTC_IO_TOUCH_XPD_BIAS_S);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);
    //clear touch enable
    WRITE_PERI_REG(SENS_SAR_TOUCH_ENABLE_REG, 0x0);
    SET_PERI_REG_MASK(RTC_CNTL_STATE0_REG, RTC_CNTL_TOUCH_SLP_TIMER_EN);
    __touchSetCycles(__touchMeasureCycles, __touchSleepCycles);
    esp_intr_alloc(ETS_RTC_CORE_INTR_SOURCE, (int)ARDUINO_ISR_FLAG, __touchISR, NULL, &touch_intr_handle);
#else
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V5);
    touch_pad_set_idle_channel_connect(TOUCH_PAD_CONN_GND);
    __touchSetCycles(__touchMeasureCycles, __touchSleepCycles);
    touch_pad_denoise_t denoise = {
        .grade = TOUCH_PAD_DENOISE_BIT4,
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();
#endif
}

uint16_t __touchRead(uint8_t pin)
{
    int8_t pad = digitalPinToTouchChannel(pin);
    if(pad < 0){
        return 0;
    }

    pinMode(pin, ANALOG);

    __touchInit();

#if CONFIG_IDF_TARGET_ESP32
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
#else
    static uint32_t chan_mask = 0;
    uint32_t value = 0;
    if((chan_mask & (1 << pad)) == 0){
        if(touch_pad_set_thresh((touch_pad_t)pad, TOUCH_PAD_THRESHOLD_MAX) != ESP_OK){
        	log_e("touch_pad_set_thresh failed");
        } else if(touch_pad_config((touch_pad_t)pad) != ESP_OK){
        	log_e("touch_pad_config failed");
        } else {
        	chan_mask |= (1 << pad);
        }
    }
    if((chan_mask & (1 << pad)) != 0) {
        if(touch_pad_read_raw_data((touch_pad_t)pad, &value) != ESP_OK){
        	log_e("touch_pad_read_raw_data failed");
        }
    }
    return value;
#endif
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

#if CONFIG_IDF_TARGET_ESP32
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
#else

#endif
}

extern uint16_t touchRead(uint8_t pin) __attribute__ ((weak, alias("__touchRead")));
extern void touchAttachInterrupt(uint8_t pin, void (*userFunc)(void), uint16_t threshold) __attribute__ ((weak, alias("__touchAttachInterrupt")));
extern void touchSetCycles(uint16_t measure, uint16_t sleep) __attribute__ ((weak, alias("__touchSetCycles")));
#endif
